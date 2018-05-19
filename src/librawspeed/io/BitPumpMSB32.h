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

#include "common/Common.h" // for uint32, uchar8
#include "io/BitStream.h"  // for BitStream, BitStreamCacheRightInLeftOut
#include "io/Endianness.h" // for getLE

namespace rawspeed {

struct MSB32BitPumpTag;

// The MSB data is ordered in MSB bit order,
// i.e. we push into the cache from the right and read it from the left

using BitPumpMSB32 = BitStream<MSB32BitPumpTag, BitStreamCacheRightInLeftOut>;

template <> struct BitStreamTraits<BitPumpMSB32> final {
  static constexpr bool canUseWithHuffmanTable = true;
};

template <>
inline BitPumpMSB32::size_type BitPumpMSB32::fillCache(const uchar8* input)
{
  static_assert(BitStreamCacheBase::MaxGetBits >= 32, "check implementation");

  cache.push(getLE<uint32>(input), 32);
  return 4;
}

} // namespace rawspeed
