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
#include "bitstreams/BitStream.h"
#include "bitstreams/BitStreamMSB16.h"
#include "bitstreams/BitStreamer.h"
#include "io/Endianness.h"
#include <cstdint>

namespace rawspeed {

class BitStreamerMSB16;

template <> struct BitStreamerTraits<BitStreamerMSB16> final {
  using Stream = BitStreamMSB16;

  // How many bytes can we read from the input per each fillCache(), at most?
  static constexpr int MaxProcessBytes = 4;
  static_assert(MaxProcessBytes == 2 * sizeof(uint16_t));
};

// The MSB data is ordered in MSB bit order,
// i.e. we push into the cache from the right and read it from the left

class BitStreamerMSB16 final : public BitStreamer<BitStreamerMSB16> {
  using Base = BitStreamer<BitStreamerMSB16>;

  friend void Base::fill(int); // Allow it to call our `fillCache()`.

  size_type fillCache(Array1DRef<const uint8_t> input);

public:
  using Base::Base;
};

inline BitStreamerMSB16::size_type
BitStreamerMSB16::fillCache(Array1DRef<const uint8_t> input) {
  static_assert(BitStreamCacheBase::MaxGetBits >= 32, "check implementation");
  establishClassInvariants();
  invariant(input.size() == Traits::MaxProcessBytes);

  constexpr int StreamChunkBitwidth =
      bitwidth<typename StreamTraits::ChunkType>();
  static_assert(CHAR_BIT * Traits::MaxProcessBytes >= StreamChunkBitwidth);
  static_assert(CHAR_BIT * Traits::MaxProcessBytes % StreamChunkBitwidth == 0);
  constexpr int NumChunksNeeded =
      (CHAR_BIT * Traits::MaxProcessBytes) / StreamChunkBitwidth;
  static_assert(NumChunksNeeded >= 1);

  for (int i = 0; i != NumChunksNeeded; ++i) {
    auto chunkInput =
        input.getBlock(sizeof(typename StreamTraits::ChunkType), i);
    auto chunk = getByteSwapped<typename StreamTraits::ChunkType>(
        chunkInput.begin(),
        StreamTraits::ChunkEndianness != getHostEndianness());
    cache.push(chunk, StreamChunkBitwidth);
  }
  return Traits::MaxProcessBytes;
}

} // namespace rawspeed
