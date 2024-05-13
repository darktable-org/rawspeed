/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2024 Roman Lebedev

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

#include "adt/Invariant.h"
#include "bitstreams/BitStream.h"
#include "io/Endianness.h"
#include <concepts>
#include <cstdint>
#include <iterator>
#include <type_traits>

namespace rawspeed {

template <typename BIT_STREAM> struct BitVacuumerTraits;

template <typename Derived_, typename OutputIterator_>
  requires std::output_iterator<OutputIterator_, uint8_t>
class BitVacuumer {
public:
  using Traits = BitVacuumerTraits<Derived_>;
  using StreamTraits = BitStreamTraits<Traits::Tag>;

  using Cache = typename StreamTraits::StreamFlow;

  using Derived = Derived_;
  using cache_type = Cache;
  using OutputIterator = OutputIterator_;

  Cache cache;

  OutputIterator output;

  bool flushed = false;

  using chunk_type = uint32_t;
  static constexpr int chunk_bitwidth = 32;

  void drainImpl() {
    invariant(cache.fillLevel >= chunk_bitwidth);
    static_assert(chunk_bitwidth == 32);

    constexpr int StreamChunkBitwidth =
        bitwidth<typename StreamTraits::ChunkType>();
    static_assert(chunk_bitwidth >= StreamChunkBitwidth);
    static_assert(chunk_bitwidth % StreamChunkBitwidth == 0);
    constexpr int NumChunksNeeded = chunk_bitwidth / StreamChunkBitwidth;
    static_assert(NumChunksNeeded >= 1);

    for (int i = 0; i != NumChunksNeeded; ++i) {
      auto chunk = implicit_cast<typename StreamTraits::ChunkType>(
          cache.peek(StreamChunkBitwidth));
      chunk = getByteSwapped<typename StreamTraits::ChunkType>(
          &chunk, StreamTraits::ChunkEndianness != Endianness::little);
      cache.skip(StreamChunkBitwidth);

      *output = chunk;
      ++output;
    }
  }

  void drain() {
    invariant(!flushed);

    if (cache.fillLevel < chunk_bitwidth)
      return; // NOTE: does not mean the cache is empty!

    static_cast<Derived*>(this)->drainImpl();
    invariant(cache.fillLevel < chunk_bitwidth);
  }

  void flush() {
    drain();

    if (cache.fillLevel == 0) {
      flushed = true;
      return;
    }

    // Pad with zero bits, so we can drain the partial chunk.
    put(/*bits=*/0, chunk_bitwidth - cache.fillLevel);
    invariant(cache.fillLevel == chunk_bitwidth);

    drain();

    invariant(cache.fillLevel == 0);
    flushed = true;
  }

  BitVacuumer() = delete;

  BitVacuumer(const BitVacuumer&) = delete;
  BitVacuumer(BitVacuumer&&) = delete;
  BitVacuumer& operator=(const BitVacuumer&) = delete;
  BitVacuumer& operator=(BitVacuumer&&) = delete;

  template <typename U>
    requires std::same_as<OutputIterator, std::remove_reference_t<U>>
  explicit BitVacuumer(U&& output_) : output(std::forward<U>(output_)) {}

  ~BitVacuumer() { flush(); }

  void put(uint32_t bits, int count) {
    invariant(count >= 0);
    // NOTE: count may be zero!
    drain();
    cache.push(bits, count);
  }
};

} // namespace rawspeed
