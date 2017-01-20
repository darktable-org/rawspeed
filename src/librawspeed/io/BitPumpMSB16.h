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

struct MSB16BitPumpTag;

// The MSB data is ordered in MSB bit order,
// i.e. we push into the cache from the right and read it from the left

using BitPumpMSB16 = BitStream<MSB16BitPumpTag, BitStreamCacheRightInLeftOut>;

template<> inline void BitPumpMSB16::fillCache() {
  static_assert(BitStreamCacheBase::MaxGetBits >= 32, "if the structure of the bit cache changed, this code has to be updated");

  for (int i = 0; i < 2; ++i) {
    cache.push(loadMem<ushort16>(data+pos, getHostEndianness() == big), 16);
    pos += 2;
  }
}

} // namespace RawSpeed
