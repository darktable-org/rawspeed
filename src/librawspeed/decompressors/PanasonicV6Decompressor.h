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

namespace rawspeed {

template <int B>
class PanasonicV6Decompressor final : public AbstractDecompressor {
  RawImage mRaw;

  ByteStream input;

  static constexpr int BitsPerSample = B;
  static_assert(BitsPerSample == 14 || BitsPerSample == 12,
                "invalid bits per sample; only use 12/14 bits.");
  static constexpr bool is14Bit = BitsPerSample == 14;

  static constexpr int PixelsPerBlock = is14Bit ? 11 : 14;
  static constexpr unsigned int PixelbaseZero = is14Bit ? 0x200 : 0x80;
  static constexpr unsigned int PixelbaseCompare = is14Bit ? 0x2000 : 0x800;
  static constexpr unsigned int SpixCompare = is14Bit ? 0xffff : 0x3fff;
  static constexpr unsigned int PixelMask = is14Bit ? 0x3fff : 0xfff;
  static constexpr int BytesPerBlock = 16;

  inline void __attribute__((always_inline))
  // NOLINTNEXTLINE(bugprone-exception-escape): no exceptions will be thrown.
  decompressBlock(ByteStream& rowInput, int row, int col) const noexcept;

  // NOLINTNEXTLINE(bugprone-exception-escape): no exceptions will be thrown.
  void decompressRow(int row) const noexcept;

public:
  PanasonicV6Decompressor(const RawImage& img, const ByteStream& input_);

  void decompress() const;
};

} // namespace rawspeed
