/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2014 Pedro Côrte-Real
    Copyright (C) 2017 Roman Lebedev

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

#include "common/Common.h"                      // for ushort16
#include "common/RawImage.h"                    // for RawImage
#include "decompressors/AbstractDecompressor.h" // for AbstractDecompressor
#include "io/ByteStream.h"                      // for ByteStream
#include <array>                                // for array

namespace rawspeed {

class KodakDecompressor final : public AbstractDecompressor {
  RawImage mRaw;
  ByteStream input;
  bool uncorrectedRawValues;

  static constexpr int segment_size = 256; // pixels
  using segment = std::array<ushort16, segment_size>;

  segment decodeSegment(uint32 bsize);

public:
  KodakDecompressor(const RawImage& img, ByteStream bs,
                    bool uncorrectedRawValues_);

  void decompress();
};

} // namespace rawspeed
