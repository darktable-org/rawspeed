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

#include "rawspeedconfig.h"
#include "adt/Array1DRef.h"
#include "adt/Bit.h"
#include "adt/Casts.h"
#include "adt/Invariant.h"
#include "bitstreams/BitStream.h"
#include "bitstreams/BitStreamJPEG.h"
#include "bitstreams/BitStreamer.h"
#include "io/Endianness.h"
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <numeric>

namespace rawspeed {

template <typename T>
  requires std::signed_integral<T>
class PosOrUnknown final {
  T val = -1; // Start with unknown position.

public:
  PosOrUnknown() = default;

  [[nodiscard]] bool has_value() const RAWSPEED_READONLY { return val >= 0; }

  template <typename U>
    requires std::same_as<U, T>
  PosOrUnknown& operator=(U newValue) {
    invariant(!has_value());
    val = newValue;
    invariant(has_value());
    return *this;
  }

  template <typename U>
    requires std::same_as<U, T>
  [[nodiscard]] T value_or(U fallback) const {
    if (has_value())
      return val;
    return fallback;
  }
};

class BitStreamerJPEG;

template <> struct BitStreamerTraits<BitStreamerJPEG> final {
  static constexpr BitOrder Tag = BitOrder::JPEG;

  static constexpr bool canUseWithPrefixCodeDecoder = true;

  // How many bytes can we read from the input per each fillCache(), at most?
  // Normally, we want to read 4 bytes, but at worst each one of those can be
  // an 0xFF byte, separated by 0x00 byte, signifying that 0xFF is a data byte.
  static constexpr int MaxProcessBytes = 8;
  static_assert(MaxProcessBytes == sizeof(uint64_t));
};

// The JPEG data is ordered in MSB bit order,
// i.e. we push into the cache from the right and read it from the left
class BitStreamerJPEG final : public BitStreamer<BitStreamerJPEG> {
  using Base = BitStreamer<BitStreamerJPEG>;

  PosOrUnknown<size_type> endOfStreamPos;

  friend void Base::fill(int nbits); // Allow it to call our `fillCache()`.

  size_type fillCache(
      std::array<std::byte, BitStreamerTraits<BitStreamerJPEG>::MaxProcessBytes>
          input);

public:
  using Base::Base;

  [[nodiscard]] size_type getStreamPosition() const;
};

// NOTE: on average, probability of encountering an `0xFF` byte
// is ~0.51% (1 in ~197), only ~2.02% (1 in ~50) of 4-byte blocks will contain
// an `0xFF` byte, and out of *those* blocks, only ~0.77% (1 in ~131)
// will contain more than one `0xFF` byte.

inline BitStreamerJPEG::size_type BitStreamerJPEG::fillCache(
    std::array<std::byte, BitStreamerTraits<BitStreamerJPEG>::MaxProcessBytes>
        inputStorage) {
  static_assert(BitStreamCacheBase::MaxGetBits >= 32, "check implementation");
  establishClassInvariants();
  auto input = Array1DRef<std::byte>(inputStorage.data(),
                                     implicit_cast<int>(inputStorage.size()));
  invariant(input.size() == Traits::MaxProcessBytes);

  constexpr int StreamChunkBitwidth =
      bitwidth<typename StreamTraits::ChunkType>();

  auto speculativeOptimisticCache = cache;
  auto speculativeOptimisticChunk =
      getByteSwapped<typename StreamTraits::ChunkType>(
          input.begin(), StreamTraits::ChunkEndianness != getHostEndianness());
  speculativeOptimisticCache.push(speculativeOptimisticChunk,
                                  StreamChunkBitwidth);

  // short-cut path for the most common case (no FF marker in the next 4 bytes)
  // this is slightly faster than the else-case alone.
  if (std::accumulate(&input(0), &input(4), true, [](bool b, std::byte byte) {
        return b && (byte != std::byte{0xFF});
      })) {
    cache = speculativeOptimisticCache;
    return 4;
  }

  size_type p = 0;
  for (size_type i = 0; i < 4; ++i) {
    const int numBytesNeeded = 4 - i;

    // Pre-execute most common case, where next byte is 'normal'/non-FF
    const std::byte c0 = input(p + 0);
    cache.push(std::to_integer<uint8_t>(c0), 8);
    if (c0 != std::byte{0xFF}) {
      p += 1;
      continue; // Got normal byte.
    }

    // Found FF -> pre-execute case of FF/00, which represents an FF data byte
    const std::byte c1 = input(p + 1);
    if (c1 == std::byte{0x00}) {
      // Got FF/00, where 0x00 is a stuffing byte (that should be ignored),
      // so 0xFF is a normal byte. All good.
      p += 2;
      continue;
    }

    // Found FF/xx with xx != 00. This is the end of stream marker.
    endOfStreamPos = getInputPosition() + p;

    // That means we shouldn't have pushed last 8 bits (0xFF, from c0).
    // We need to "unpush" them, and fill the vacant cache bits with zeros.

    // First, recover the cache fill level.
    cache.fillLevel -= 8;
    // Now, this code is incredibly underencapsulated, and the
    // implementation details are leaking into here. Thus, we know that
    // all the fillLevel bits in cache are all high bits. So to "unpush"
    // the last 8 bits, and fill the vacant cache bits with zeros, we only
    // need to keep the high fillLevel bits. So just create a mask with only
    // high fillLevel bits set, and 'and' the cache with it.
    // Caution, we know fillLevel won't be 64, but it may be 0,
    // so pick the mask-creation idiom accordingly.
    cache.cache &= ~((~0ULL) >> cache.fillLevel);
    cache.fillLevel = 64;

    // No further reading from this buffer shall happen. Do signal that by
    // claiming that we have consumed all the remaining bytes of the buffer.

    p = getRemainingSize() + numBytesNeeded;
    invariant(p >= 6);
    break;
  }
  invariant(p >= 5);
  return p;
}

inline BitStreamerJPEG::size_type BitStreamerJPEG::getStreamPosition() const {
  // The current number of bytes we consumed.
  // When at the end of the stream pos, it points to the JPEG marker FF
  return endOfStreamPos.value_or(getInputPosition());
}

} // namespace rawspeed
