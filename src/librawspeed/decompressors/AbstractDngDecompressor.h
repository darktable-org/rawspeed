/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post

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

#include "common/Common.h"                                  // for uint32
#include "decompressors/AbstractParallelizedDecompressor.h" // for Abstract...
#include "io/ByteStream.h"                                  // for ByteStream
#include <utility>                                          // for move
#include <vector>                                           // for vector

namespace rawspeed {

class RawImage;

struct DngSliceElement {
  DngSliceElement(ByteStream bs_, uint32 offsetX, uint32 offsetY, uint32 w,
                  uint32 h)
      : bs(std::move(bs_)), offX(offsetX), offY(offsetY), width(w), height(h) {}

  const ByteStream bs;
  const uint32 offX;
  const uint32 offY;
  const uint32 width;
  const uint32 height;
};

class AbstractDngDecompressor final : public AbstractParallelizedDecompressor {
  void decompressThreaded(const RawDecompressorThread* t) const final;

public:
  AbstractDngDecompressor(const RawImage& img, int compression_,
                          bool mFixLjpeg_, uint32 mBps_, uint32 mPredictor_)
      : AbstractParallelizedDecompressor(img), compression(compression_),
        mFixLjpeg(mFixLjpeg_), mBps(mBps_), mPredictor(mPredictor_) {}

  void decode() const final;

  std::vector<DngSliceElement> slices;

  const int compression;
  const bool mFixLjpeg = false;
  const uint32 mBps;
  const uint32 mPredictor;
};

} // namespace rawspeed
