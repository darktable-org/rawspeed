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

#include "rawspeedconfig.h"
#include "adt/Array1DRef.h"
#include "adt/Casts.h"
#include "adt/Invariant.h"
#include "adt/VariableLengthLoad.h"
#include "common/Common.h"
#include "io/IOException.h"
#include <array>
#include <cstdint>

namespace rawspeed {

template <typename BIT_STREAM> struct BitStreamTraits final {
  static constexpr bool canUseWithPrefixCodeDecoder = false;
};

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
  inline void push(uint64_t bits, int count) noexcept {
    establishClassInvariants();
    invariant(count >= 0);
    invariant(count != 0);
    invariant(count <= Size);
    invariant(count + fillLevel <= Size);
    cache |= bits << fillLevel;
    fillLevel += count;
  }

  [[nodiscard]] inline uint32_t peek(int count) const noexcept {
    establishClassInvariants();
    invariant(count >= 0);
    invariant(count <= MaxGetBits);
    invariant(count != 0);
    invariant(count <= Size);
    invariant(count <= fillLevel);
    invariant(count <= 31);
    return cache & ((1U << count) - 1U);
  }

  inline void skip(int count) noexcept {
    establishClassInvariants();
    invariant(count >= 0);
    // `count` *could* be larger than `MaxGetBits`.
    invariant(count != 0);
    invariant(count <= Size);
    invariant(count <= fillLevel);
    cache >>= count;
    fillLevel -= count;
  }
};

struct BitStreamCacheRightInLeftOut final : BitStreamCacheBase {
  inline void push(uint64_t bits, int count) noexcept {
    establishClassInvariants();
    invariant(count >= 0);
    invariant(count != 0);
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
    invariant(emptyBitsGap < Size);
    // So just shift the new bits so that there is no gap in the middle.
    cache |= bits << emptyBitsGap;
    fillLevel += count;
  }

  [[nodiscard]] inline auto peek(int count) const noexcept {
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

  inline void skip(int count) noexcept {
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

template <typename Tag> struct BitStreamReplenisherBase {
  using size_type = int32_t;

  Array1DRef<const uint8_t> input;
  int pos = 0;

  void establishClassInvariants() const noexcept;

  BitStreamReplenisherBase() = delete;

  inline explicit BitStreamReplenisherBase(Array1DRef<const uint8_t> input_)
      : input(input_) {
    if (input.size() < BitStreamTraits<Tag>::MaxProcessBytes)
      ThrowIOE("Bit stream size is smaller than MaxProcessBytes");
  }

  // A temporary intermediate buffer that may be used by fill() method either
  // in debug build to enforce lack of out-of-bounds reads, or when we are
  // nearing the end of the input buffer and can not just read
  // BitStreamTraits<Tag>::MaxProcessBytes from it, but have to read as much as
  // we can and fill rest with zeros.
  std::array<uint8_t, BitStreamTraits<Tag>::MaxProcessBytes> tmpStorage = {};
  inline Array1DRef<uint8_t> tmp() noexcept RAWSPEED_READONLY {
    return {tmpStorage.data(), implicit_cast<int>(tmpStorage.size())};
  }
};

template <typename Tag>
__attribute__((always_inline)) inline void
BitStreamReplenisherBase<Tag>::establishClassInvariants() const noexcept {
  input.establishClassInvariants();
  invariant(input.size() >= BitStreamTraits<Tag>::MaxProcessBytes);
  invariant(pos >= 0);
  // `pos` *could* be out-of-bounds of `input`.
}

template <typename Tag>
struct BitStreamForwardSequentialReplenisher final
    : public BitStreamReplenisherBase<Tag> {
  using Base = BitStreamReplenisherBase<Tag>;

  using Base::BitStreamReplenisherBase;

  BitStreamForwardSequentialReplenisher() = delete;

  [[nodiscard]] inline typename Base::size_type getPos() const {
    Base::establishClassInvariants();
    return Base::pos;
  }
  [[nodiscard]] inline typename Base::size_type getRemainingSize() const {
    Base::establishClassInvariants();
    return Base::input.size() - getPos();
  }
  inline void markNumBytesAsConsumed(typename Base::size_type numBytes) {
    Base::establishClassInvariants();
    invariant(numBytes >= 0);
    invariant(numBytes != 0);
    Base::pos += numBytes;
  }

  inline Array1DRef<const uint8_t> getInput() {
    Base::establishClassInvariants();

#if !defined(DEBUG)
    // Do we have BitStreamTraits<Tag>::MaxProcessBytes or more bytes left in
    // the input buffer? If so, then we can just read from said buffer.
    if (getPos() + BitStreamTraits<Tag>::MaxProcessBytes <=
        Base::input.size()) {
      return Base::input
          .getCrop(getPos(), BitStreamTraits<Tag>::MaxProcessBytes)
          .getAsArray1DRef();
    }
#endif

    // We have to use intermediate buffer, either because the input is running
    // out of bytes, or because we want to enforce bounds checking.

    // Note that in order to keep all fill-level invariants we must allow to
    // over-read past-the-end a bit.
    if (getPos() >
        Base::input.size() + 2 * BitStreamTraits<Tag>::MaxProcessBytes)
      ThrowIOE("Buffer overflow read in BitStream");

    variableLengthLoadNaiveViaMemcpy(Base::tmp(), Base::input, getPos());

    return Base::tmp();
  }
};

template <typename Tag, typename Cache,
          typename Replenisher = BitStreamForwardSequentialReplenisher<Tag>>
class BitStream final {
  Cache cache;

  Replenisher replenisher;

  using size_type = int32_t;

  // this method hase to be implemented in the concrete BitStream template
  // specializations. It will return the number of bytes processed. It needs
  // to process up to BitStreamTraits<Tag>::MaxProcessBytes bytes of input.
  size_type fillCache(Array1DRef<const uint8_t> input);

public:
  using tag = Tag;

  void establishClassInvariants() const noexcept {
    cache.establishClassInvariants();
    replenisher.establishClassInvariants();
  }

  BitStream() = delete;

  inline explicit BitStream(Array1DRef<const uint8_t> input)
      : replenisher(input) {
    establishClassInvariants();
  }

  inline void fill(int nbits = Cache::MaxGetBits) {
    establishClassInvariants();
    invariant(nbits >= 0);
    invariant(nbits != 0);
    invariant(nbits <= Cache::MaxGetBits);

    if (cache.fillLevel >= nbits)
      return;

    replenisher.markNumBytesAsConsumed(fillCache(replenisher.getInput()));
  }

  // these methods might be specialized by implementations that support it
  [[nodiscard]] inline size_type RAWSPEED_READONLY getInputPosition() const {
    establishClassInvariants();
    return replenisher.getPos();
  }

  // these methods might be specialized by implementations that support it
  [[nodiscard]] inline size_type getStreamPosition() const {
    establishClassInvariants();
    return getInputPosition() - (cache.fillLevel >> 3);
  }

  [[nodiscard]] inline size_type getRemainingSize() const {
    establishClassInvariants();
    return replenisher.getRemainingSize();
  }

  [[nodiscard]] inline size_type RAWSPEED_READONLY getFillLevel() const {
    establishClassInvariants();
    return cache.fillLevel;
  }

  inline uint32_t RAWSPEED_READONLY peekBitsNoFill(int nbits) {
    establishClassInvariants();
    invariant(nbits >= 0);
    invariant(nbits != 0);
    invariant(nbits <= Cache::MaxGetBits);
    return cache.peek(nbits);
  }

  inline void skipBitsNoFill(int nbits) {
    establishClassInvariants();
    invariant(nbits >= 0);
    // `nbits` could be zero.
    invariant(nbits <= Cache::MaxGetBits);
    cache.skip(nbits);
  }

  inline uint32_t getBitsNoFill(int nbits) {
    establishClassInvariants();
    invariant(nbits >= 0);
    invariant(nbits != 0);
    invariant(nbits <= Cache::MaxGetBits);
    uint32_t ret = peekBitsNoFill(nbits);
    skipBitsNoFill(nbits);
    return ret;
  }

  inline uint32_t peekBits(int nbits) {
    establishClassInvariants();
    invariant(nbits >= 0);
    invariant(nbits != 0);
    invariant(nbits <= Cache::MaxGetBits);
    fill(nbits);
    return peekBitsNoFill(nbits);
  }

  inline uint32_t getBits(int nbits) {
    establishClassInvariants();
    invariant(nbits >= 0);
    invariant(nbits != 0);
    invariant(nbits <= Cache::MaxGetBits);
    fill(nbits);
    return getBitsNoFill(nbits);
  }

  // This may be used to skip arbitrarily large number of *bytes*,
  // not limited by the fill level.
  inline void skipBytes(int nbytes) {
    establishClassInvariants();
    int remainingBitsToSkip = 8 * nbytes;
    for (; remainingBitsToSkip >= Cache::MaxGetBits;
         remainingBitsToSkip -= Cache::MaxGetBits) {
      fill(Cache::MaxGetBits);
      skipBitsNoFill(Cache::MaxGetBits);
    }
    if (remainingBitsToSkip > 0) {
      fill(remainingBitsToSkip);
      skipBitsNoFill(remainingBitsToSkip);
    }
  }
};

} // namespace rawspeed
