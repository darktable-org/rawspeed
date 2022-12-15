/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2022 Jordan Neumeyer

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
#include "decompressors/PanasonicV7Decompressor.h"
#include "adt/Array2DRef.h"               // for Array2DRef
#include "adt/Point.h"                    // for iPoint2D
#include "common/Common.h"                // for rawspeed_get_number_of_pro...
#include "common/RawImage.h"              // for RawImage, RawImageData
#include "decoders/RawDecoderException.h" // for ThrowException, ThrowRDE
#include <array>                          // for array
#include <cassert>                        // for assert
#include <cstdint>                        // for uint16_t
#include <functional>

namespace rawspeed {

PanasonicV7Decompressor::PanasonicV7Decompressor(const RawImage& img,
                                                 const ByteStream& input_,
                                                 const int bps_)
    : mRaw(img) {
  if (mRaw->getCpp() != 1 || mRaw->getDataType() != RawImageType::UINT16 ||
      mRaw->getBpp() != sizeof(uint16_t))
    ThrowRDE("Unexpected component count / data type");

  bitsPerSample = bps_;
  // Default to 14 bit.
  pixelsPerBlock = bps_ != 12 ? PixelsPerBlock14Bit : PixelsPerBlock12Bit;

  if (!mRaw->dim.hasPositiveArea() || mRaw->dim.x % pixelsPerBlock != 0) {
    ThrowRDE("Unexpected image dimensions found: (%i; %i)", mRaw->dim.x,
             mRaw->dim.y);
  }

  // How many blocks are needed for the given image size?
  const auto numBlocks = mRaw->dim.area() / pixelsPerBlock;

  // Does the input contain enough blocks?
  // How many full blocks does the input contain? This is truncating division.
  if (const auto haveBlocks = input_.getRemainSize() / BytesPerBlock;
      haveBlocks < numBlocks)
    ThrowRDE("Insufficient count of input blocks for a given image");

  // We only want those blocks we need, no extras.
  input = input_.peekStream(numBlocks, BytesPerBlock);
}

inline uint16_t
PanasonicV7Decompressor::streamedPixelRead(const ByteStream& bs,
                                           int pixelpos) noexcept {
  switch (pixelpos) {
  case 0:
    return bs.peekByte(0) |                // low bits
           ((bs.peekByte(1) & 0x3F) << 8); // high bits

  case 1:
    return (bs.peekByte(1) >> 6) | (bs.peekByte(2) << 2) |
           ((bs.peekByte(3) & 0xF) << 10);

  case 2:
    return (bs.peekByte(3) >> 4) | (bs.peekByte(4) << 4) |
           ((bs.peekByte(5) & 3) << 12);

  case 3:
    return ((bs.peekByte(5) & 0xFC) >> 2) | (bs.peekByte(6) << 6);

  case 4:
    return bs.peekByte(7) | ((bs.peekByte(8) & 0x3F) << 8);

  case 5:
    return (bs.peekByte(8) >> 6) | (bs.peekByte(9) << 2) |
           ((bs.peekByte(10) & 0xF) << 10);

  case 6:
    return (bs.peekByte(10) >> 4) | (bs.peekByte(11) << 4) |
           ((bs.peekByte(12) & 3) << 12);

  case 7:
    return ((bs.peekByte(12) & 0xFC) >> 2) | (bs.peekByte(13) << 6);

  case 8:
    return bs.peekByte(14) | ((bs.peekByte(15) & 0x3F) << 8);

  default:
    // This shouldn't happen.
    return 0;
  }
}

inline uint16_t
PanasonicV7Decompressor::streamedPixelRead12Bit(const ByteStream& bs,
                                                int pixelpos) noexcept {
  switch (pixelpos) {
  case 0:
    return ((bs.peekByte(1) & 0xF) << 8) | bs.peekByte(0);

  case 1:
    return (bs.peekByte(2) << 4) | (bs.peekByte(1) >> 4);

  case 2:
    return ((bs.peekByte(4) & 0xF) << 8) | bs.peekByte(3);

  case 3:
    return (bs.peekByte(5) << 4) | (bs.peekByte(4) >> 4);

  case 4:
    return ((bs.peekByte(7) & 0xF) << 8) | bs.peekByte(6);

  case 5:
    return (bs.peekByte(8) << 4) | (bs.peekByte(7) >> 4);

  case 6:
    return ((bs.peekByte(10) & 0xF) << 8) | bs.peekByte(9);

  case 7:
    return (bs.peekByte(11) << 4) | (bs.peekByte(10) >> 4);

  case 8:
    return ((bs.peekByte(13) & 0xF) << 8) | bs.peekByte(12);

  case 9:
    return (bs.peekByte(14) << 4) | (bs.peekByte(13) >> 4);
  default:
    // This shouldn't happen.
    return 0;
  }
}

inline void __attribute__((always_inline))
// NOLINTNEXTLINE(bugprone-exception-escape): no exceptions will be thrown.
PanasonicV7Decompressor::decompressBlock(
    ByteStream& rowInput, int row, int col,
    const std::function<uint16_t(const ByteStream&, int)>& readPixelFn)
    const noexcept {
  const Array2DRef<uint16_t> out(mRaw->getU16DataAsUncroppedArray2DRef());
  const auto stream =
      rowInput.getStream(PanasonicV7Decompressor::BytesPerBlock);

  for (int pix = 0; pix < pixelsPerBlock; pix++, col++) {
    out(row, col) = readPixelFn(stream, pix);
  }
}

// NOLINTNEXTLINE(bugprone-exception-escape): no exceptions will be thrown.
void PanasonicV7Decompressor::decompressRow(int row) const noexcept {
  assert(mRaw->dim.x % pixelsPerBlock == 0);
  const int blocksperrow = mRaw->dim.x / pixelsPerBlock;
  const int bytesPerRow = PanasonicV7Decompressor::BytesPerBlock * blocksperrow;

  // Default to 14 bit.
  const auto readPixelFn =
      bitsPerSample != 12 ? streamedPixelRead : streamedPixelRead12Bit;

  ByteStream rowInput = input.getSubStream(bytesPerRow * row, bytesPerRow);
  for (int rblock = 0, col = 0; rblock < blocksperrow;
       rblock++, col += pixelsPerBlock) {
    decompressBlock(rowInput, row, col, readPixelFn);
  }
}

void PanasonicV7Decompressor::decompress() const {
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

} // namespace rawspeed
