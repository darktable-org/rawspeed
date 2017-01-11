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

#include "BitStream.h"

namespace RawSpeed {

struct MSB32BitPumpTag;

// The MSB data is ordered in MSB bit order,
// i.e. we push into the cache from the right and read it from the left

template<> inline void BitStream<MSB32BitPumpTag>::fillCache() {
  static_assert(sizeof(cache) == 8 && MaxGetBits <= 32, "if the structure of the bit cache changed, this code has to be updated");

  cache = cache << 32 | loadMem<uint32>(data+pos, getHostEndianness() == big);
  pos += 4;
  bitsInCache += 32;
}

template<> inline uint32 BitStream<MSB32BitPumpTag>::peekCacheBits(uint32 nbits) const {
  return peekCacheBitsR2L(nbits);
}

template<> inline void BitStream<MSB32BitPumpTag>::skipCacheBits(uint32 nbits) {
  skipCacheBitsR2L(nbits);
}

using BitPumpMSB32 = BitStream<MSB32BitPumpTag>;

} // namespace RawSpeed
