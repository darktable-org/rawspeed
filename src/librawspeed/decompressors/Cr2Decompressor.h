/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2017 Axel Waggershauser
    Copyright (C) 2018 Roman Lebedev

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

#pragma once

#include "common/Common.h"                           // for uint16_t
#include "decoders/RawDecoderException.h"            // for ThrowRDE
#include "decompressors/AbstractLJpegDecompressor.h" // for AbstractLJpegDe...
#include <cassert>                                   // for assert

namespace rawspeed {

class ByteStream;
class RawImage;

class Cr2Slicing {
  int numSlices = 0;
  int sliceWidth = 0;
  int lastSliceWidth = 0;

  friend class Cr2Decompressor;

public:
  Cr2Slicing() = default;

  Cr2Slicing(uint16_t numSlices_, uint16_t sliceWidth_,
             uint16_t lastSliceWidth_)
      : numSlices(numSlices_), sliceWidth(sliceWidth_),
        lastSliceWidth(lastSliceWidth_) {
    if (numSlices < 1)
      ThrowRDE("Bad slice count: %u", numSlices);
  }

  bool empty() const {
    return 0 == numSlices && 0 == sliceWidth && 0 == lastSliceWidth;
  }

  unsigned widthOfSlice(int sliceId) const {
    assert(sliceId >= 0);
    assert(sliceId < numSlices);
    if ((sliceId + 1) == numSlices)
      return lastSliceWidth;
    return sliceWidth;
  }

  unsigned totalWidth() const {
    int width = 0;
    for (auto sliceId = 0; sliceId < numSlices; sliceId++)
      width += widthOfSlice(sliceId);
    return width;
  }
};

class Cr2Decompressor final : public AbstractLJpegDecompressor
{
  Cr2Slicing slicing;

  void decodeScan() override;
  template<int N_COMP, int X_S_F, int Y_S_F> void decodeN_X_Y();

public:
  Cr2Decompressor(const ByteStream& bs, const RawImage& img);
  void decode(const Cr2Slicing& slicing);
};

} // namespace rawspeed
