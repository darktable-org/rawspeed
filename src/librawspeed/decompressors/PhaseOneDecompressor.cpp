/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2014-2015 Pedro Côrte-Real
    Copyright (C) 2017-2018 Roman Lebedev

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include "decompressors/PhaseOneDecompressor.h"
#include "common/Common.h"                // for int32, uint32, ushort16
#include "common/Point.h"                 // for iPoint2D
#include "common/RawImage.h"              // for RawImage, RawImageData
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "io/BitPumpMSB32.h"              // for BitPumpMSB32
#include <array>                          // for array
#include <cassert>                        // for assert
#include <stddef.h>                       // for size_t
#include <utility>                        // for move
#include <vector>                         // for vector, vector<>::size_type

namespace rawspeed {

PhaseOneDecompressor::PhaseOneDecompressor(const RawImage& img,
                                           std::vector<PhaseOneStrip>&& strips_)
    : AbstractParallelizedDecompressor(img), strips(std::move(strips_)) {
  if (mRaw->getDataType() != TYPE_USHORT16)
    ThrowRDE("Unexpected data type");

  if (!((mRaw->getCpp() == 1 && mRaw->getBpp() == 2)))
    ThrowRDE("Unexpected cpp: %u", mRaw->getCpp());

  if (!mRaw->dim.hasPositiveArea() || mRaw->dim.x % 2 != 0 ||
      mRaw->dim.x > 11608 || mRaw->dim.y > 8708) {
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", mRaw->dim.x,
             mRaw->dim.y);
  }

  if (strips.size() != static_cast<decltype(strips)::size_type>(mRaw->dim.y)) {
    ThrowRDE("Height (%u) vs strip count %zu mismatch", mRaw->dim.y,
             strips.size());
  }
}

void PhaseOneDecompressor::decompressStrip(const PhaseOneStrip& strip) const {
  uint32 width = mRaw->dim.x;
  assert(width % 2 == 0);

  static constexpr std::array<const int, 10> length = {8,  7, 6,  9,  11,
                                                       10, 5, 12, 14, 13};

  BitPumpMSB32 pump(strip.bs);

  std::array<int32, 2> pred;
  pred.fill(0);
  std::array<int, 2> len;
  auto* img = reinterpret_cast<ushort16*>(mRaw->getData(0, strip.n));
  for (uint32 col = 0; col < width; col++) {
    if (col >= (width & -8))
      len[0] = len[1] = 14;
    else if ((col & 7) == 0) {
      for (int& i : len) {
        int j = 0;

        for (; j < 5; j++) {
          if (pump.getBits(1) != 0) {
            if (col == 0)
              ThrowRDE("Can not initialize lengths. Data is corrupt.");

            // else, we have previously initialized lengths, so we are fine
            break;
          }
        }

        assert((col == 0 && j > 0) || col != 0);
        if (j > 0)
          i = length[2 * (j - 1) + pump.getBits(1)];
      }
    }

    int i = len[col & 1];
    if (i == 14)
      img[col] = pred[col & 1] = pump.getBits(16);
    else {
      pred[col & 1] +=
          static_cast<signed>(pump.getBits(i)) + 1 - (1 << (i - 1));
      // FIXME: is the truncation the right solution here?
      img[col] = ushort16(pred[col & 1]);
    }
  }
}

void PhaseOneDecompressor::decompressThreaded(
    const RawDecompressorThread* t) const {
  for (size_t i = t->start; i < t->end && i < strips.size(); i++)
    decompressStrip(strips[i]);
}

} // namespace rawspeed
