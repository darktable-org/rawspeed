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

#pragma once

#include "common/RawImage.h"
#include "decompressors/AbstractDecompressor.h"
#include "io/ByteStream.h"
#include <climits>
#include <cstdint>

namespace rawspeed {
template <class T> class CroppedArray1DRef;

class PanasonicV7Decompressor final : public AbstractDecompressor {
  RawImage mRaw;

  ByteStream input;

  static constexpr int BytesPerBlock = 16;
  static constexpr int BitsPerSample = 14;
  static constexpr int PixelsPerBlock =
      (CHAR_BIT * BytesPerBlock) / BitsPerSample;

  static inline void __attribute__((always_inline))
  decompressBlock(ByteStream block, CroppedArray1DRef<uint16_t> out) noexcept;

  void decompressRow(int row) const noexcept;

public:
  PanasonicV7Decompressor(RawImage img, ByteStream input_);

  void decompress() const;
};

} // namespace rawspeed
