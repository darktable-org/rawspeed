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
#include <cstdint>
#include <iterator>

namespace rawspeed {

template <typename BIT_STREAM> struct BitVacuumerTraits;

template <typename Derived_, typename OutputIterator_>
  requires std::output_iterator<OutputIterator_, uint8_t>
class BitVacuumer {
  using Traits = BitVacuumerTraits<Derived_>;
  using StreamTraits = BitStreamTraits<typename Traits::Stream>;

  using Cache = typename StreamTraits::StreamFlow;

public:
  using Derived = Derived_;
  using cache_type = Cache;
  using OutputIterator = OutputIterator_;

  Cache cache;

  OutputIterator output;

  bool flushed = false;

  using chunk_type = uint32_t;
  static constexpr int chunk_bitwidth = 32;

  inline void drain() {
    invariant(!flushed);

    if (cache.fillLevel < chunk_bitwidth)
      return; // NOTE: does not mean the cache is empty!

    static_cast<Derived*>(this)->drainImpl();
    invariant(cache.fillLevel < chunk_bitwidth);
  }

  inline void flush() {
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

  inline explicit BitVacuumer(OutputIterator output_) : output(output_) {}

  inline ~BitVacuumer() { flush(); }

  inline void put(uint32_t bits, int count) {
    invariant(count >= 0);
    if (count == 0)
      return; // No-op.
    drain();
    cache.push(bits, count);
  }
};

} // namespace rawspeed
