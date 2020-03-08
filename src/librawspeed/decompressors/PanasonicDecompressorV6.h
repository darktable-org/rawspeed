/*
    RawSpeed - RAW file decoder.

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

#pragma once

#include "common/RawImage.h"                    // for RawImage
#include "decompressors/AbstractDecompressor.h" // for AbstractDecompressor
#include "io/ByteStream.h"                      // for ByteStream
#include <cstdint>                              // for uint32_t

namespace rawspeed {

class PanasonicDecompressorV6 final : public AbstractDecompressor {
  RawImage mRaw;

  ByteStream input;

  static constexpr int PixelsPerBlock = 11;
  static constexpr int BytesPerBlock = 16;

  void decompressBlock(ByteStream* rowInput, int row, int col) const;
  void decompressRow(int row) const;

public:
  PanasonicDecompressorV6(const RawImage& img, ByteStream input_);

  void decompress() const;
};

} // namespace rawspeed
