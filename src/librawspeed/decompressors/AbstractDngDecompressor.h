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
#include <vector>                                           // for vector

namespace rawspeed {

class Buffer;
class RawImage;

class DngSliceElement {
public:
  DngSliceElement(uint32 off, uint32 count, uint32 offsetX, uint32 offsetY,
                  uint32 w, uint32 h)
      : byteOffset(off), byteCount(count), offX(offsetX), offY(offsetY),
        width(w), height(h) {}
  const uint32 byteOffset;
  const uint32 byteCount;
  const uint32 offX;
  const uint32 offY;
  const uint32 width;
  const uint32 height;
};

class AbstractDngDecompressor final : public AbstractParallelizedDecompressor {
  void decompressThreaded(const RawDecompressorThread* t) const final;

public:
  AbstractDngDecompressor(const Buffer* file, const RawImage& img,
                          int compression);

  void addSlice(DngSliceElement slice);

  void decode() const final;

  int __attribute__((pure)) size();
  std::vector<DngSliceElement> slices;

  const Buffer* mFile;
  bool mFixLjpeg;
  uint32 mPredictor;
  uint32 mBps;
  int compression;
};

} // namespace rawspeed
