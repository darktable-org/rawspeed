/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2017 Axel Waggershauser

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

#include "io/BitStream.h" // for BitStreamCacheRightInLeftOut, BitStream
#include "io/Buffer.h"     // for Buffer::size_type
#include "io/Endianness.h" // for getBE
#include <cstdint>         // for uint32_t, uint8_t

namespace rawspeed {

struct MSBBitPumpTag;

// The MSB data is ordered in MSB bit order,
// i.e. we push into the cache from the right and read it from the left

using BitPumpMSB = BitStream<MSBBitPumpTag, BitStreamCacheRightInLeftOut>;

template <> struct BitStreamTraits<MSBBitPumpTag> final {
  static constexpr bool canUseWithHuffmanTable = true;

  // How many bytes can we read from the input per each fillCache(), at most?
  static constexpr int MaxProcessBytes = 4;
};

template <>
inline BitPumpMSB::size_type BitPumpMSB::fillCache(const uint8_t* input) {
  static_assert(BitStreamCacheBase::MaxGetBits >= 32, "check implementation");

  cache.push(getBE<uint32_t>(input), 32);
  return 4;
}

} // namespace rawspeed
