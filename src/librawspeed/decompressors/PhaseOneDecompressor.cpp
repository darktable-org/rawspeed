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

#include "decompressors/PhaseOneDecompressor.h" // for PhaseOneDecompressor
#include "common/Common.h"                      // for uint32, ushort16, int32
#include "common/Point.h"                       // for iPoint2D
#include "common/Spline.h" // for Spline, Spline<>::value_type
#include "decoders/IiqDecoder.h"
#include "decoders/RawDecoder.h"          // for RawDecoder::(anonymous)
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "io/BitPumpMSB32.h"              // for BitPumpMSB32
#include "io/Buffer.h"                    // for Buffer, DataBuffer
#include "io/ByteStream.h"                // for ByteStream
#include "io/Endianness.h"                // for Endianness, Endianness::li...
#include "tiff/TiffIFD.h"                 // for TiffRootIFD, TiffID
#include <algorithm>                      // for adjacent_find, generate_n
#include <array>                          // for array, array<>::const_iter...
#include <cassert>                        // for assert
#include <functional>                     // for greater_equal
#include <iterator>                       // for advance, next, begin, end
#include <memory>                         // for unique_ptr
#include <string>                         // for operator==, string
#include <utility>                        // for move
#include <vector>                         // for vector

namespace rawspeed {

PhaseOneDecompressor::PhaseOneDecompressor(const RawImage& img,
                                           std::vector<PhaseOneStrip>&& strips_)
    : mRaw(img), strips(std::move(strips_)) {
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

void PhaseOneDecompressor::decompressStrip(const PhaseOneStrip& strip) {
  uint32 width = mRaw->dim.x;
  assert(width % 2 == 0);

  const int length[] = {8, 7, 6, 9, 11, 10, 5, 12, 14, 13};

  BitPumpMSB32 pump(strip.bs);

  int32 pred[2];
  uint32 len[2];
  pred[0] = pred[1] = 0;
  auto* img = reinterpret_cast<ushort16*>(mRaw->getData(0, strip.n));
  for (uint32 col = 0; col < width; col++) {
    if (col >= (width & -8))
      len[0] = len[1] = 14;
    else if ((col & 7) == 0) {
      for (unsigned int& i : len) {
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

void PhaseOneDecompressor::decompress() {
  for (const auto& strip : strips)
    decompressStrip(strip);
}

} // namespace rawspeed
