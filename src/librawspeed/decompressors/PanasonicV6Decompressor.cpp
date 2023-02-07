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
#include "decompressors/PanasonicV6Decompressor.h"
#include "adt/Array2DRef.h"               // for Array2DRef
#include "adt/Point.h"                    // for iPoint2D
#include "common/Common.h"                // for rawspeed_get_number_of_pro...
#include "common/RawImage.h"              // for RawImage, RawImageData
#include "decoders/RawDecoderException.h" // for ThrowException, ThrowRDE
#include <array>                          // for array
#include <cassert>                        // for assert
#include <cstdint>                        // for uint16_t

namespace rawspeed {

namespace {
template <const int B> struct pana_cs6_page_decoder {
  static_assert((B == 14 || B == 12), "only 12/14 bits are valid!");
  static constexpr int BufferSize = B == 14 ? 14 : 18;
  std::array<uint16_t, BufferSize> pixelbuffer;
  unsigned char current = 0;

  void fillBuffer(const ByteStream& bs) noexcept;

  explicit pana_cs6_page_decoder(const ByteStream& bs) { fillBuffer(bs); }

  uint16_t nextpixel() {
    uint16_t currPixel = pixelbuffer[current];
    ++current;
    return currPixel;
  }
};

template <>
inline void __attribute__((always_inline))
pana_cs6_page_decoder<12>::fillBuffer(const ByteStream& bs) noexcept {
  // 12 bit: 8/0 + 4 upper bits of /1
  pixelbuffer[0] = (bs.peekByte(15) << 4) | (bs.peekByte(14) >> 4);
  // 12 bit: 4l/1 + 8/2
  pixelbuffer[1] = (((bs.peekByte(14) & 0xf) << 8) | (bs.peekByte(13))) & 0xfff;

  // 2; 2u/3, 6 low bits remains in bs.peekByte(12)
  pixelbuffer[2] = (bs.peekByte(12) >> 6) & 0x3;
  // 8; 6l/3 + 2u/4; 6 low bits remains in bs.peekByte(11)
  pixelbuffer[3] = ((bs.peekByte(12) & 0x3f) << 2) | (bs.peekByte(11) >> 6);
  // 8: 6l/4 + 2u/5; 6 low bits remains in bs.peekByte(10)
  pixelbuffer[4] = ((bs.peekByte(11) & 0x3f) << 2) | (bs.peekByte(10) >> 6);
  // 8: 6l/5 + 2u/6, 6 low bits remains in bs.peekByte(9)
  pixelbuffer[5] = ((bs.peekByte(10) & 0x3f) << 2) | (bs.peekByte(9) >> 6);

  // 2, 4 low bits remains in bs.peekByte(9)
  pixelbuffer[6] = (bs.peekByte(9) >> 4) & 0x3;
  // 8: 4 low bits from bs.peekByte(9), 4 upper bits from bs.peekByte(8)
  pixelbuffer[7] = ((bs.peekByte(9) & 0xf) << 4) | (bs.peekByte(8) >> 4);
  // 8: 4 low bits from bs.peekByte(8), 4 upper bits from bs.peekByte(7)
  pixelbuffer[8] = ((bs.peekByte(8) & 0xf) << 4) | (bs.peekByte(7) >> 4);
  // 8: 4 low bits from bs.peekByte(7), 4 upper bits from bs.peekByte(6)
  pixelbuffer[9] = ((bs.peekByte(7) & 0xf) << 4) | (bs.peekByte(6) >> 4);

  // 2: bits 2-3 from bs.peekByte(6), two low bits remain in bs.peekByte(6)
  pixelbuffer[10] = (bs.peekByte(6) >> 2) & 0x3;
  // 2: bits 2-3 from bs.peekByte(6), two low bits remain in bs.peekByte(6)
  pixelbuffer[11] = ((bs.peekByte(6) & 0x3) << 6) | (bs.peekByte(5) >> 2);
  // 8: 2 bits from bs.peekByte(5), 6 bits from bs.peekByte(4)
  pixelbuffer[12] = ((bs.peekByte(5) & 0x3) << 6) | (bs.peekByte(4) >> 2);
  // 8: 2 bits from bs.peekByte(4), 6 bits from bs.peekByte(3)
  pixelbuffer[13] = ((bs.peekByte(4) & 0x3) << 6) | (bs.peekByte(3) >> 2);

  // 2: low bits from bs.peekByte(3)
  pixelbuffer[14] = bs.peekByte(3) & 0x3;
  pixelbuffer[15] = bs.peekByte(2);
  pixelbuffer[16] = bs.peekByte(1);
  pixelbuffer[17] = bs.peekByte(0);
}

template <>
inline void __attribute__((always_inline))
pana_cs6_page_decoder<14>::fillBuffer(const ByteStream& bs) noexcept {
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

} // namespace

template <int B>
PanasonicV6Decompressor<B>::PanasonicV6Decompressor(const RawImage& img,
                                                    const ByteStream& input_)
    : mRaw(img) {
  if (mRaw->getCpp() != 1 || mRaw->getDataType() != RawImageType::UINT16 ||
      mRaw->getBpp() != sizeof(uint16_t))
    ThrowRDE("Unexpected component count / data type");

  if (!mRaw->dim.hasPositiveArea() ||
      mRaw->dim.x % PanasonicV6Decompressor::PixelsPerBlock != 0) {
    ThrowRDE("Unexpected image dimensions found: (%i; %i)", mRaw->dim.x,
             mRaw->dim.y);
  }

  // How many blocks are needed for the given image size?
  const auto numBlocks = mRaw->dim.area() / PixelsPerBlock;

  // Does the input contain enough blocks?
  // How many full blocks does the input contain? This is truncating division.
  if (const auto haveBlocks = input_.getRemainSize() / BytesPerBlock;
      haveBlocks < numBlocks)
    ThrowRDE("Insufficient count of input blocks for a given image");

  // We only want those blocks we need, no extras.
  input = input_.peekStream(numBlocks, BytesPerBlock);
}

template <int B>
inline void __attribute__((always_inline))
// NOLINTNEXTLINE(bugprone-exception-escape): no exceptions will be thrown.
PanasonicV6Decompressor<B>::decompressBlock(ByteStream& rowInput, int row,
                                            int col) const noexcept {
  const Array2DRef<uint16_t> out(mRaw->getU16DataAsUncroppedArray2DRef());

  pana_cs6_page_decoder<B> page(
      rowInput.getStream(PanasonicV6Decompressor::BytesPerBlock));

  std::array<unsigned, 2> oddeven = {0, 0};
  std::array<unsigned, 2> nonzero = {0, 0};
  unsigned pmul = 0;
  unsigned pixel_base = 0;
  for (int pix = 0; pix < PanasonicV6Decompressor::PixelsPerBlock;
       pix++, col++) {
    if (pix % 3 == 2) {
      uint16_t base = page.nextpixel();
      if (base == 3)
        base = 4;
      pixel_base = PanasonicV6Decompressor::PixelbaseZero << base;
      pmul = 1 << base;
    }
    uint16_t epixel = page.nextpixel();
    if (oddeven[pix % 2]) {
      epixel *= pmul;
      if (pixel_base < PanasonicV6Decompressor::PixelbaseCompare &&
          nonzero[pix % 2] > pixel_base)
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
    if (spix <= PanasonicV6Decompressor::SpixCompare)
      out(row, col) = spix & PanasonicV6Decompressor::SpixCompare;
    else {
      // FIXME: this is a convoluted way to compute zero.
      // What was this code trying to do, actually?
      epixel = static_cast<int>(epixel + 0x7ffffff1) >> 0x1f;
      out(row, col) = epixel & PanasonicV6Decompressor::PixelMask;
    }
  }
}

// NOLINTNEXTLINE(bugprone-exception-escape): no exceptions will be thrown.
template <int B>
void PanasonicV6Decompressor<B>::decompressRow(int row) const noexcept {
  assert(mRaw->dim.x % PanasonicV6Decompressor::PixelsPerBlock == 0);
  const int blocksperrow =
      mRaw->dim.x / PanasonicV6Decompressor::PixelsPerBlock;
  const int bytesPerRow = PanasonicV6Decompressor::BytesPerBlock * blocksperrow;

  ByteStream rowInput = input.getSubStream(bytesPerRow * row, bytesPerRow);
  for (int rblock = 0, col = 0; rblock < blocksperrow;
       rblock++, col += PanasonicV6Decompressor::PixelsPerBlock)
    decompressBlock(rowInput, row, col);
}

template <int B> void PanasonicV6Decompressor<B>::decompress() const {
#ifdef HAVE_OPENMP
#pragma omp parallel for num_threads(rawspeed_get_number_of_processor_cores()) \
    schedule(static) default(none)
#endif
  for (int row = 0; row < mRaw->dim.y;
       ++row) { // NOLINT(openmp-exception-escape): we know no exceptions will
                // be thrown.
    decompressRow(row);
  }
}

// forward refs.
template class PanasonicV6Decompressor<14>;
template class PanasonicV6Decompressor<12>;

} // namespace rawspeed
