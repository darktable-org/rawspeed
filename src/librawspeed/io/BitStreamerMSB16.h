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

#include "adt/Array1DRef.h"
#include "adt/Invariant.h"
#include "io/BitStream.h"
#include "io/BitStreamer.h"
#include "io/Endianness.h"
#include <cstdint>

namespace rawspeed {

class BitStreamerMSB16;

template <> struct BitStreamerTraits<BitStreamerMSB16> final {
  // How many bytes can we read from the input per each fillCache(), at most?
  static constexpr int MaxProcessBytes = 4;
  static_assert(MaxProcessBytes == 2 * sizeof(uint16_t));
};

// The MSB data is ordered in MSB bit order,
// i.e. we push into the cache from the right and read it from the left

class BitStreamerMSB16 final
    : public BitStreamer<BitStreamerMSB16, BitStreamCacheRightInLeftOut> {
  using Base = BitStreamer<BitStreamerMSB16, BitStreamCacheRightInLeftOut>;

  friend void Base::fill(int); // Allow it to call our `fillCache()`.

  size_type fillCache(Array1DRef<const uint8_t> input);

public:
  using Base::Base;
};

inline BitStreamerMSB16::size_type
BitStreamerMSB16::fillCache(Array1DRef<const uint8_t> input) {
  static_assert(BitStreamCacheBase::MaxGetBits >= 32, "check implementation");
  establishClassInvariants();
  invariant(input.size() ==
            BitStreamerTraits<BitStreamerMSB16>::MaxProcessBytes);

  for (size_type i = 0; i < 4; i += sizeof(uint16_t)) {
    cache.push(getLE<uint16_t>(input.getCrop(i, sizeof(uint16_t)).begin()), 16);
  }
  return 4;
}

} // namespace rawspeed
