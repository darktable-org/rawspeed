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

#include "io/BitStream.h"

namespace RawSpeed {

struct MSB32BitPumpTag;

// The MSB data is ordered in MSB bit order,
// i.e. we push into the cache from the right and read it from the left

using BitPumpMSB32 = BitStream<MSB32BitPumpTag, BitStreamCacheRightInLeftOut>;

template<> inline void BitPumpMSB32::fillCache() {
  static_assert(BitStreamCacheBase::MaxGetBits >= 32, "if the structure of the bit cache changed, this code has to be updated");

  cache.push(getLE<uint32>(data + pos), 32);
  pos += 4;
}

} // namespace RawSpeed
