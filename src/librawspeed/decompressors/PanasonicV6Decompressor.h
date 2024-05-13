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

#include "common/RawImage.h"
#include "decompressors/AbstractDecompressor.h"
#include "io/ByteStream.h"
#include <cstdint>

namespace rawspeed {

class PanasonicV6Decompressor final : public AbstractDecompressor {
  RawImage mRaw;

  ByteStream input;

  // Contains the decoding recepie for the block,
  struct BlockDsc;

  // There are two variants. Which one is to be used depends on image's bps.
  static const BlockDsc TwelveBitBlock;
  static const BlockDsc FourteenBitBlock;

  const uint32_t bps;

  template <const BlockDsc& dsc>
  inline void __attribute__((always_inline))
  decompressBlock(ByteStream& rowInput, int row, int col) const noexcept;

  template <const BlockDsc& dsc> void decompressRow(int row) const noexcept;

  template <const BlockDsc& dsc> void decompressInternal() const noexcept;

public:
  PanasonicV6Decompressor(RawImage img, ByteStream input_, uint32_t bps_);

  void decompress() const noexcept;
};

} // namespace rawspeed
