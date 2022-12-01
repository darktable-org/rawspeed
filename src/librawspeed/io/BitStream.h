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

#include "common/Common.h"  // for bitwidth, extractHighBits
#include "io/Buffer.h"      // for Buffer
#include "io/ByteStream.h"  // for ByteStream
#include "io/Endianness.h"  // for Endianness, Endianness::unknown
#include "io/IOException.h" // for ThrowIOE
#include <algorithm>        // for fill_n, min
#include <array>            // for array
#include <cassert>          // for assert
#include <cstdint>          // for uint32_t, uint8_t, uint64_t
#include <cstring>          // for memcpy

namespace rawspeed {

template <typename BIT_STREAM> struct BitStreamTraits final {
  static constexpr bool canUseWithHuffmanTable = false;
};

// simple 64-bit wide cache implementation that acts like a FiFo.
// There are two variants:
//  * L->R: new bits are pushed in on the left and pulled out on the right
//  * L<-R: new bits are pushed in on the right and pulled out on the left
// Each BitStream specialization uses one of the two.

struct BitStreamCacheBase
{
  uint64_t cache = 0;         // the actual bits stored in the cache
  unsigned int fillLevel = 0; // bits left in cache

  static constexpr unsigned Size = bitwidth<decltype(cache)>();

  // how many bits could be requested to be filled
  static constexpr unsigned MaxGetBits = bitwidth<uint32_t>();
};

struct BitStreamCacheLeftInRightOut : BitStreamCacheBase
{
  inline void push(uint64_t bits, uint32_t count) noexcept {
    assert(count + fillLevel <= bitwidth(cache));
    cache |= bits << fillLevel;
    fillLevel += count;
  }

  [[nodiscard]] inline uint32_t peek(uint32_t count) const noexcept {
    return cache & ((1U << count) - 1U);
  }

  inline void skip(uint32_t count) noexcept {
    cache >>= count;
    fillLevel -= count;
  }
};

struct BitStreamCacheRightInLeftOut : BitStreamCacheBase
{
  inline void push(uint64_t bits, uint32_t count) noexcept {
    assert(count + fillLevel <= Size);
    assert(count != 0);
    // If the maximal size of the cache is BitStreamCacheBase::Size, and we
    // have fillLevel [high] bits set, how many empty [low] bits do we have?
    const uint32_t vacantBits = BitStreamCacheBase::Size - fillLevel;
    // If we just directly 'or' these low bits into the cache right now,
    // how many unfilled bits of a gap will there be in the middle of a cache?
    const uint32_t emptyBitsGap = vacantBits - count;
    // So just shift the new bits so that there is no gap in the middle.
    cache |= bits << emptyBitsGap;
    fillLevel += count;
  }

  [[nodiscard]] inline uint32_t peek(uint32_t count) const noexcept {
    return extractHighBits(cache, count, /*effectiveBitwidth=*/BitStreamCacheBase::Size);
  }

  inline void skip(uint32_t count) noexcept {
    fillLevel -= count;
    cache <<= count;
  }
};

template <typename Tag> struct BitStreamReplenisherBase {
  using size_type = uint32_t;

  const uint8_t* data;
  size_type size;
  unsigned pos = 0;

  BitStreamReplenisherBase() = default;

  explicit BitStreamReplenisherBase(const Buffer& input)
      : data(input.getData(0, input.getSize())), size(input.getSize()) {
    if (size < BitStreamTraits<Tag>::MaxProcessBytes)
      ThrowIOE("Bit stream size is smaller than MaxProcessBytes");
  }

  // A temporary intermediate buffer that may be used by fill() method either
  // in debug build to enforce lack of out-of-bounds reads, or when we are
  // nearing the end of the input buffer and can not just read
  // BitStreamTraits<Tag>::MaxProcessBytes from it, but have to read as much as
  // we can and fill rest with zeros.
  std::array<uint8_t, BitStreamTraits<Tag>::MaxProcessBytes> tmp = {};
};

template <typename Tag>
struct BitStreamForwardSequentialReplenisher final
    : public BitStreamReplenisherBase<Tag> {
  using Base = BitStreamReplenisherBase<Tag>;

  BitStreamForwardSequentialReplenisher() = default;

  using Base::BitStreamReplenisherBase;

  [[nodiscard]] inline typename Base::size_type getPos() const {
    return Base::pos;
  }
  [[nodiscard]] inline typename Base::size_type getRemainingSize() const {
    return Base::size - getPos();
  }
  inline void markNumBytesAsConsumed(typename Base::size_type numBytes) {
    Base::pos += numBytes;
  }

  inline const uint8_t* getInput() {
#if !defined(DEBUG)
    // Do we have BitStreamTraits<Tag>::MaxProcessBytes or more bytes left in
    // the input buffer? If so, then we can just read from said buffer.
    if (Base::pos + BitStreamTraits<Tag>::MaxProcessBytes <= Base::size)
      return Base::data + Base::pos;
#endif

    // We have to use intermediate buffer, either because the input is running
    // out of bytes, or because we want to enforce bounds checking.

    // Note that in order to keep all fill-level invariants we must allow to
    // over-read past-the-end a bit.
    if (Base::pos > Base::size + 2 * BitStreamTraits<Tag>::MaxProcessBytes)
      ThrowIOE("Buffer overflow read in BitStream");

    Base::tmp.fill(0);

    // How many bytes are left in input buffer?
    // Since pos can be past-the-end we need to carefully handle overflow.
    typename Base::size_type bytesRemaining =
        (Base::pos < Base::size) ? Base::size - Base::pos : 0;
    // And if we are not at the end of the input, we may have more than we need.
    bytesRemaining = std::min<typename Base::size_type>(
        BitStreamTraits<Tag>::MaxProcessBytes, bytesRemaining);

    memcpy(Base::tmp.data(), Base::data + Base::pos, bytesRemaining);
    return Base::tmp.data();
  }
};

template <typename Tag, typename Cache,
          typename Replenisher = BitStreamForwardSequentialReplenisher<Tag>>
class BitStream final {
  Cache cache;

  Replenisher replenisher;

  using size_type = uint32_t;

  // this method hase to be implemented in the concrete BitStream template
  // specializations. It will return the number of bytes processed. It needs
  // to process up to BitStreamTraits<Tag>::MaxProcessBytes bytes of input.
  size_type fillCache(const uint8_t* input);

public:
  using tag = Tag;

  BitStream() = default;

  explicit BitStream(const Buffer& buf) : replenisher(buf) {}

  explicit BitStream(const ByteStream& s)
      : BitStream(s.getSubView(s.getPosition(), s.getRemainSize())) {}

  inline void fill(uint32_t nbits = Cache::MaxGetBits) {
    assert(nbits <= Cache::MaxGetBits);

    if (cache.fillLevel >= nbits)
      return;

    replenisher.markNumBytesAsConsumed(fillCache(replenisher.getInput()));
  }

  // these methods might be specialized by implementations that support it
  [[nodiscard]] inline size_type getInputPosition() const {
    return replenisher.getPos();
  }

  // these methods might be specialized by implementations that support it
  [[nodiscard]] inline size_type getStreamPosition() const {
    return getInputPosition() - (cache.fillLevel >> 3);
  }

  [[nodiscard]] inline size_type getRemainingSize() const {
    return replenisher.getRemainingSize();
  }

  [[nodiscard]] inline size_type getFillLevel() const {
    return cache.fillLevel;
  }

  inline uint32_t __attribute__((pure)) peekBitsNoFill(uint32_t nbits) {
    assert(nbits != 0);
    assert(nbits < Cache::MaxGetBits);
    assert(nbits <= cache.fillLevel);
    return cache.peek(nbits);
  }

  inline void skipBitsNoFill(uint32_t nbits) {
    assert(nbits <= Cache::MaxGetBits);
    assert(nbits <= cache.fillLevel);
    cache.skip(nbits);
  }

  inline uint32_t getBitsNoFill(uint32_t nbits) {
    uint32_t ret = peekBitsNoFill(nbits);
    skipBitsNoFill(nbits);
    return ret;
  }

  inline uint32_t peekBits(uint32_t nbits) {
    fill(nbits);
    return peekBitsNoFill(nbits);
  }

  inline uint32_t getBits(uint32_t nbits) {
    fill(nbits);
    return getBitsNoFill(nbits);
  }

  // This may be used to skip arbitrarily large number of *bytes*,
  // not limited by the fill level.
  inline void skipBytes(uint32_t nbytes) {
    uint32_t remainingBitsToSkip = 8 * nbytes;
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
