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

#include "rawspeedconfig.h" // for HAVE_OPENMP
#include "decompressors/PanasonicDecompressorV6.h"
#include "common/Array2DRef.h"            // for Array2DRef
#include "common/Common.h"                // for rawspeed_get_number_of_pro...
#include "common/Point.h"                 // for iPoint2D
#include "common/RawImage.h"              // for RawImageData, RawImage
#include "common/RawspeedException.h"     // for RawspeedException
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include <array>                          // for array
#include <cassert>                        // for assert
#include <cstdint>                        // for uint16_t
#include <string>                         // for string
#include <utility>                        // for move

namespace rawspeed {

constexpr int PanasonicDecompressorV6::PixelsPerBlock;
constexpr int PanasonicDecompressorV6::BytesPerBlock;

namespace {
struct pana_cs6_page_decoder {
  std::array<uint16_t, 14> pixelbuffer;
  unsigned char current = 0;

  explicit pana_cs6_page_decoder(const ByteStream& bs) {
    // The bit packing scheme here is actually just 128-bit little-endian int,
    // that we consume from the high bits to low bits, with no padding.
    // It is really tempting to refactor this using proper BitPump, but so far
    // that results in disappointing performance.

    // 14 bits
    pixelbuffer[0] = (bs.peekByte(15) << 6) | (bs.peekByte(14) >> 2);
    // 14 bits
    pixelbuffer[1] = (((bs.peekByte(14) & 0x3) << 12) | (bs.peekByte(13) << 4) |
                      (bs.peekByte(12) >> 4)) &
                     0x3fff;
    // 2 bits
    pixelbuffer[2] = (bs.peekByte(12) >> 2) & 0x3;
    // 10 bits
    pixelbuffer[3] = ((bs.peekByte(12) & 0x3) << 8) | bs.peekByte(11);
    // 10 bits
    pixelbuffer[4] = (bs.peekByte(10) << 2) | (bs.peekByte(9) >> 6);
    // 10 bits
    pixelbuffer[5] = ((bs.peekByte(9) & 0x3f) << 4) | (bs.peekByte(8) >> 4);
    // 2 bits
    pixelbuffer[6] = (bs.peekByte(8) >> 2) & 0x3;
    // 10 bits
    pixelbuffer[7] = ((bs.peekByte(8) & 0x3) << 8) | bs.peekByte(7);
    // 10 bits
    pixelbuffer[8] = ((bs.peekByte(6) << 2) & 0x3fc) | (bs.peekByte(5) >> 6);
    // 10 bits
    pixelbuffer[9] = ((bs.peekByte(5) << 4) | (bs.peekByte(4) >> 4)) & 0x3ff;
    // 2 bits
    pixelbuffer[10] = (bs.peekByte(4) >> 2) & 0x3;
    // 10 bits
    pixelbuffer[11] = ((bs.peekByte(4) & 0x3) << 8) | bs.peekByte(3);
    // 10 bits
    pixelbuffer[12] =
        (((bs.peekByte(2) << 2) & 0x3fc) | bs.peekByte(1) >> 6) & 0x3ff;
    // 10 bits
    pixelbuffer[13] = ((bs.peekByte(1) << 4) | (bs.peekByte(0) >> 4)) & 0x3ff;
    // 4 padding bits
  }

  uint16_t nextpixel() { return pixelbuffer[current++]; }
};
} // namespace

PanasonicDecompressorV6::PanasonicDecompressorV6(const RawImage& img,
                                                 const ByteStream& input_)
    : mRaw(img) {
  if (mRaw->getCpp() != 1 || mRaw->getDataType() != TYPE_USHORT16 ||
      mRaw->getBpp() != sizeof(uint16_t))
    ThrowRDE("Unexpected component count / data type");

  if (!mRaw->dim.hasPositiveArea() ||
      mRaw->dim.x % PanasonicDecompressorV6::PixelsPerBlock != 0) {
    ThrowRDE("Unexpected image dimensions found: (%i; %i)", mRaw->dim.x,
             mRaw->dim.y);
  }

  // How many blocks are needed for the given image size?
  const auto numBlocks = mRaw->dim.area() / PixelsPerBlock;

  // How many full blocks does the input contain? This is truncating division.
  const auto haveBlocks = input_.getRemainSize() / BytesPerBlock;

  // Does the input contain enough blocks?
  if (haveBlocks < numBlocks)
    ThrowRDE("Insufficient count of input blocks for a given image");

  // We only want those blocks we need, no extras.
  input = input_.peekStream(numBlocks, BytesPerBlock);
}

void PanasonicDecompressorV6::decompressBlock(ByteStream* rowInput, int row,
                                              int col) const {
  const Array2DRef<uint16_t> out(mRaw->getU16DataAsUncroppedArray2DRef());

  pana_cs6_page_decoder page(
      rowInput->getStream(PanasonicDecompressorV6::BytesPerBlock));

  std::array<unsigned, 2> oddeven = {0, 0};
  std::array<unsigned, 2> nonzero = {0, 0};
  unsigned pmul = 0;
  unsigned pixel_base = 0;
  for (int pix = 0; pix < PanasonicDecompressorV6::PixelsPerBlock;
       pix++, col++) {
    if (pix % 3 == 2) {
      uint16_t base = page.nextpixel();
      if (base > 3)
        ThrowRDE("Invariant failure");
      if (base == 3)
        base = 4;
      pixel_base = 0x200 << base;
      pmul = 1 << base;
    }
    uint16_t epixel = page.nextpixel();
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
      out(row, col) = spix & 0xffff;
    else {
      epixel = static_cast<int>(epixel + 0x7ffffff1) >> 0x1f;
      out(row, col) = epixel & 0x3fff;
    }
  }
}

void PanasonicDecompressorV6::decompressRow(int row) const {
  assert(mRaw->dim.x % PanasonicDecompressorV6::PixelsPerBlock == 0);
  const int blocksperrow =
      mRaw->dim.x / PanasonicDecompressorV6::PixelsPerBlock;
  const int bytesPerRow = PanasonicDecompressorV6::BytesPerBlock * blocksperrow;

  ByteStream rowInput = input.getSubStream(bytesPerRow * row, bytesPerRow);
  for (int rblock = 0, col = 0; rblock < blocksperrow;
       rblock++, col += PanasonicDecompressorV6::PixelsPerBlock)
    decompressBlock(&rowInput, row, col);
}

void PanasonicDecompressorV6::decompress() const {
#ifdef HAVE_OPENMP
#pragma omp parallel for num_threads(rawspeed_get_number_of_processor_cores()) \
    schedule(static) default(none)
#endif
  for (int row = 0; row < mRaw->dim.y; ++row) {
    try {
      decompressRow(row);
    } catch (RawspeedException& err) {
      // Propagate the exception out of OpenMP magic.
      mRaw->setError(err.what());
    }
  }

  std::string firstErr;
  if (mRaw->isTooManyErrors(1, &firstErr)) {
    ThrowRDE("Too many errors encountered. Giving up. First Error:\n%s",
             firstErr.c_str());
  }
}

} // namespace rawspeed
