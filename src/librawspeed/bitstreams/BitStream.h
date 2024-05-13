/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2017 Axel Waggershauser
    Copyright (C) 2017-2021 Roman Lebedev

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

#include "adt/Bit.h"
#include "adt/Casts.h"
#include "adt/Invariant.h"
#include "bitstreams/BitStreams.h"
#include <cstdint>

namespace rawspeed {

template <BitOrder bo> struct BitStreamTraits;

// simple 64-bit wide cache implementation that acts like a FiFo.
// There are two variants:
//  * L->R: new bits are pushed in on the left and pulled out on the right
//  * L<-R: new bits are pushed in on the right and pulled out on the left
// Each BitStream specialization uses one of the two.

struct BitStreamCacheBase {
  uint64_t cache = 0; // the actual bits stored in the cache
  int fillLevel = 0;  // bits left in cache

  static constexpr int Size = bitwidth<decltype(cache)>();

  // how many bits could be requested to be filled
  static constexpr int MaxGetBits = bitwidth<uint32_t>();

  void establishClassInvariants() const noexcept;
};

__attribute__((always_inline)) inline void
BitStreamCacheBase::establishClassInvariants() const noexcept {
  invariant(fillLevel >= 0);
  invariant(fillLevel <= Size);
}

struct BitStreamCacheLeftInRightOut final : BitStreamCacheBase {
  void push(uint64_t bits, int count) noexcept {
    establishClassInvariants();
    invariant(count >= 0);
    // NOTE: count may be zero!
    invariant(count <= Size);
    invariant(count + fillLevel <= Size);
    cache |= bits << fillLevel;
    fillLevel += count;
  }

  [[nodiscard]] uint32_t peek(int count) const noexcept {
    establishClassInvariants();
    invariant(count >= 0);
    invariant(count <= MaxGetBits);
    invariant(count != 0);
    invariant(count <= Size);
    invariant(count <= fillLevel);
    return extractLowBits(static_cast<uint32_t>(cache), count);
  }

  void skip(int count) noexcept {
    establishClassInvariants();
    invariant(count >= 0);
    // `count` *could* be larger than `MaxGetBits`.
    // `count` could be zero.
    invariant(count <= Size);
    invariant(count <= fillLevel);
    cache >>= count;
    fillLevel -= count;
  }
};

struct BitStreamCacheRightInLeftOut final : BitStreamCacheBase {
  void push(uint64_t bits, int count) noexcept {
    establishClassInvariants();
    invariant(count >= 0);
    // NOTE: count may be zero!
    invariant(count <= Size);
    invariant(count + fillLevel <= Size);
    // If the maximal size of the cache is BitStreamCacheBase::Size, and we
    // have fillLevel [high] bits set, how many empty [low] bits do we have?
    const int vacantBits = BitStreamCacheBase::Size - fillLevel;
    invariant(vacantBits >= 0);
    invariant(vacantBits <= Size);
    invariant(vacantBits != 0);
    invariant(vacantBits >= count);
    // If we just directly 'or' these low bits into the cache right now,
    // how many unfilled bits of a gap will there be in the middle of a cache?
    const int emptyBitsGap = vacantBits - count;
    invariant(emptyBitsGap >= 0);
    invariant(emptyBitsGap <= Size);
    if (count != 0) {
      invariant(emptyBitsGap < Size);
      // So just shift the new bits so that there is no gap in the middle.
      cache |= bits << emptyBitsGap;
    }
    fillLevel += count;
  }

  [[nodiscard]] auto peek(int count) const noexcept {
    establishClassInvariants();
    invariant(count >= 0);
    invariant(count <= Size);
    invariant(count <= MaxGetBits);
    invariant(count != 0);
    invariant(count <= fillLevel);
    return implicit_cast<uint32_t>(
        extractHighBits(cache, count,
                        /*effectiveBitwidth=*/BitStreamCacheBase::Size));
  }

  void skip(int count) noexcept {
    establishClassInvariants();
    invariant(count >= 0);
    // `count` *could* be larger than `MaxGetBits`.
    // `count` could be zero.
    invariant(count <= Size);
    invariant(count <= fillLevel);
    fillLevel -= count;
    cache <<= count;
  }
};

} // namespace rawspeed
