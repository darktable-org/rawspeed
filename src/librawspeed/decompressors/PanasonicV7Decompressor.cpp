/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2022 LibRaw LLC (info@libraw.org)
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
#include "io/BitPumpLSB.h"
#include <array>                          // for array
#include <cassert>                        // for assert
#include <cstdint>                        // for uint16_t
#include <functional>

namespace rawspeed {

PanasonicV7Decompressor::PanasonicV7Decompressor(const RawImage& img,
                                                 const ByteStream& input_)
    : mRaw(img) {
  if (mRaw->getCpp() != 1 || mRaw->getDataType() != RawImageType::UINT16 ||
      mRaw->getBpp() != sizeof(uint16_t))
    ThrowRDE("Unexpected component count / data type");

  if (!mRaw->dim.hasPositiveArea() || mRaw->dim.x % PixelsPerBlock != 0) {
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

inline void __attribute__((always_inline))
// NOLINTNEXTLINE(bugprone-exception-escape): no exceptions will be thrown.
PanasonicV7Decompressor::decompressBlock(const ByteStream& block, int row,
                                         int col) const noexcept {
  const Array2DRef<uint16_t> out(mRaw->getU16DataAsUncroppedArray2DRef());
  BitPumpLSB pump(block);
  for (int pix = 0; pix < PixelsPerBlock; pix++, col++)
    out(row, col) = pump.getBits(BitsPerSample);
}

// NOLINTNEXTLINE(bugprone-exception-escape): no exceptions will be thrown.
void PanasonicV7Decompressor::decompressRow(int row) const noexcept {
  assert(mRaw->dim.x % PixelsPerBlock == 0);
  const int blocksperrow = mRaw->dim.x / PixelsPerBlock;
  const int bytesPerRow = BytesPerBlock * blocksperrow;

  ByteStream rowInput = input.getSubStream(bytesPerRow * row, bytesPerRow);
  for (int rblock = 0, col = 0; rblock < blocksperrow;
       rblock++, col += PixelsPerBlock) {
    ByteStream block = rowInput.getStream(BytesPerBlock);
    decompressBlock(block, row, col);
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
