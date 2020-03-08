/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2019 LibRaw LLC (info@libraw.org)
    Copyright (C) 2020 Roman Lebedev

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include "decompressors/PanasonicDecompressorV6.h" // for PanasonicDecompre...
#include "common/Point.h"                          // for iPoint2D
#include "common/RawImage.h"                       // for RawImage, RawImag...
#include "decoders/RawDecoderException.h"          // for ThrowRDE
#include <algorithm>                               // for copy_n
#include <array>
#include <cstdint> // for uint16_t, uint32_t
#include <cstdlib> // for free, malloc

namespace rawspeed {

namespace {
struct pana_cs6_page_decoder {
  std::array<unsigned int, 14> pixelbuffer;
  unsigned lastoffset, maxoffset;
  unsigned char current;
  const unsigned char* buffer;
  pana_cs6_page_decoder(const unsigned char* _buffer, unsigned int bsize)
      : lastoffset(0), maxoffset(bsize), current(0), buffer(_buffer) {}
  void read_page(); // will throw IO error if not enough space in buffer
  unsigned int nextpixel() { return current < 14 ? pixelbuffer[current++] : 0; }
};

void pana_cs6_page_decoder::read_page() {
  if (!buffer || (maxoffset - lastoffset < 16))
    ThrowRDE("Input failure");
  auto wbuffer = [&](int i) {
    return static_cast<uint16_t>(buffer[lastoffset + 15 - i]);
  };
  pixelbuffer[0] = (wbuffer(0) << 6) | (wbuffer(1) >> 2); // 14 bit
  pixelbuffer[1] =
      (((wbuffer(1) & 0x3) << 12) | (wbuffer(2) << 4) | (wbuffer(3) >> 4)) &
      0x3fff;
  pixelbuffer[2] = (wbuffer(3) >> 2) & 0x3;
  pixelbuffer[3] = ((wbuffer(3) & 0x3) << 8) | wbuffer(4);
  pixelbuffer[4] = (wbuffer(5) << 2) | (wbuffer(6) >> 6);
  pixelbuffer[5] = ((wbuffer(6) & 0x3f) << 4) | (wbuffer(7) >> 4);
  pixelbuffer[6] = (wbuffer(7) >> 2) & 0x3;
  pixelbuffer[7] = ((wbuffer(7) & 0x3) << 8) | wbuffer(8);
  pixelbuffer[8] = ((wbuffer(9) << 2) & 0x3fc) | (wbuffer(10) >> 6);
  pixelbuffer[9] = ((wbuffer(10) << 4) | (wbuffer(11) >> 4)) & 0x3ff;
  pixelbuffer[10] = (wbuffer(11) >> 2) & 0x3;
  pixelbuffer[11] = ((wbuffer(11) & 0x3) << 8) | wbuffer(12);
  pixelbuffer[12] = (((wbuffer(13) << 2) & 0x3fc) | wbuffer(14) >> 6) & 0x3ff;
  pixelbuffer[13] = ((wbuffer(14) << 4) | (wbuffer(15) >> 4)) & 0x3ff;
#undef wbuffer
  current = 0;
  lastoffset += 16;
}
} // namespace

PanasonicDecompressorV6::PanasonicDecompressorV6(const RawImage& img,
                                                 ByteStream input_,
                                                 uint32_t bps_)
    : mRaw(img), input(std::move(input_)) {
  if (mRaw->getCpp() != 1 || mRaw->getDataType() != TYPE_USHORT16 ||
      mRaw->getBpp() != sizeof(uint16_t))
    ThrowRDE("Unexpected component count / data type");

  if (!mRaw->dim.hasPositiveArea()) {
    ThrowRDE("Unexpected image dimensions found: (%i; %i)", mRaw->dim.x,
             mRaw->dim.y);
  }
}

void PanasonicDecompressorV6::decompressBlock(int row, int col) {
  auto* rowptr = reinterpret_cast<uint16_t*>(mRaw->getDataUncropped(0, row));
  pana_cs6_page_decoder page(input.getData(16), 16);
  page.read_page();
  std::array<unsigned int, 2> oddeven = {0, 0};
  std::array<unsigned int, 2> nonzero = {0, 0};
  unsigned pmul = 0;
  unsigned pixel_base = 0;
  for (int pix = 0; pix < 11; pix++) {
    if (pix % 3 == 2) {
      unsigned base = page.nextpixel();
      if (base > 3)
        ThrowRDE("Invariant failure");
      if (base == 3)
        base = 4;
      pixel_base = 0x200 << base;
      pmul = 1 << base;
    }
    unsigned epixel = page.nextpixel();
    if (oddeven[pix % 2]) {
      epixel *= pmul;
      if (pixel_base < 0x2000 && nonzero[pix % 2] > pixel_base)
        epixel += nonzero[pix % 2] - pixel_base;
      nonzero[pix % 2] = epixel;
    } else {
      oddeven[pix % 2] = epixel;
      if (epixel)
        nonzero[pix % 2] = epixel;
      else
        epixel = nonzero[pix % 2];
    }
    auto spix = static_cast<unsigned>(static_cast<int>(epixel) - 0xf);
    if (spix <= 0xffff)
      rowptr[col++] = spix & 0xffff;
    else {
      epixel = static_cast<signed int>(epixel + 0x7ffffff1) >> 0x1f;
      rowptr[col++] = epixel & 0x3fff;
    }
  }
}

void PanasonicDecompressorV6::decompressRow(int row) {
  const int blocksperrow = mRaw->dim.x / 11;

  for (int rblock = 0, col = 0; rblock < blocksperrow; rblock++, col += 11)
    decompressBlock(row, col);
}

void PanasonicDecompressorV6::decompress() {
  for (int row = 0; row < mRaw->dim.y; ++row)
    decompressRow(row);
}

} // namespace rawspeed
