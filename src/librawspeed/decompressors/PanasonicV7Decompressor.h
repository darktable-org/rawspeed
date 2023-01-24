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

#include "common/RawImage.h"                    // for RawImage
#include "decompressors/AbstractDecompressor.h" // for AbstractDecompressor
#include "io/ByteStream.h"                      // for ByteStream
#include <functional>

namespace rawspeed {

class PanasonicV7Decompressor final : public AbstractDecompressor {
  RawImage mRaw;

  ByteStream input;

  int pixelsPerBlock;
  int bitsPerSample;

  static constexpr int PixelsPerBlock14Bit = 9;
  static constexpr int PixelsPerBlock12Bit = 10;

  static constexpr int BytesPerBlock = 16;

  inline void __attribute__((always_inline))
  // NOLINTNEXTLINE(bugprone-exception-escape): no exceptions will be thrown.
  decompressBlock(ByteStream& rowInput, int row, int col, 
                  const std::function<uint16_t(const ByteStream&, int)>& readPixel) const noexcept;

  // NOLINTNEXTLINE(bugprone-exception-escape): no exceptions will be thrown.
  void decompressRow(int row) const noexcept;

  static uint16_t streamedPixelRead(const ByteStream& bs, int pixelpos) noexcept;
  static uint16_t streamedPixelRead12Bit(const ByteStream& bs, int pixelpos) noexcept;

public:
  PanasonicV7Decompressor(const RawImage& img, const ByteStream& input_, int bps_);

  void decompress() const;
};

} // namespace rawspeed
