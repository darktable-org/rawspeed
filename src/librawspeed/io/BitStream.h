/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2017 Axel Waggershauser
    Copyright (C) 2017-2019 Roman Lebedev

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

#include "common/Common.h" // for uint32_t, uint8_t, uint64_t
#include "io/Buffer.h"     // for Buffer::size_type
#include "io/ByteStream.h"  // for ByteStream
#include "io/IOException.h" // for IOException (ptr only), ThrowIOE
#include <cassert>          // for assert
#include <cstring>          // for memcpy

namespace rawspeed {

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

  // maximal number of bytes the implementation may read.
  // NOTE: this is not the same as MaxGetBits/8 !!!
  static constexpr unsigned MaxProcessBytes = 8;
};

struct BitStreamCacheLeftInRightOut : BitStreamCacheBase
{
  inline void push(uint64_t bits, uint32_t count) noexcept {
    assert(count + fillLevel <= bitwidth(cache));
    cache |= bits << fillLevel;
    fillLevel += count;
  }

  inline uint32_t peek(uint32_t count) const noexcept {
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
    assert(count + fillLevel <= bitwidth(cache));
    assert(count < bitwidth(cache));
    cache = cache << count | bits;
    fillLevel += count;
  }

  inline uint32_t peek(uint32_t count) const noexcept {
    return (cache >> (fillLevel - count)) & ((1U << count) - 1U);
  }

  inline void skip(uint32_t count) noexcept { fillLevel -= count; }
};

template <typename BIT_STREAM> struct BitStreamTraits final {
  static constexpr bool canUseWithHuffmanTable = false;
};

template <typename Tag, typename Cache>
class BitStream final : public ByteStream {
  Cache cache;

  // A temporary intermediate buffer that may be used by fill() method either
  // in debug build to enforce lack of out-of-bounds reads, or when we are
  // nearing the end of the input buffer and can not just read MaxProcessBytes
  // from it, but have to read as much as we can and fill rest with zeros.
  std::array<uint8_t, BitStreamCacheBase::MaxProcessBytes> tmp = {};

  // this method hase to be implemented in the concrete BitStream template
  // specializations. It will return the number of bytes processed. It needs
  // to process up to BitStreamCacheBase::MaxProcessBytes bytes of input.
  size_type fillCache(const uint8_t* input, size_type bufferSize,
                      size_type* bufPos);

public:
  BitStream() = default;

  explicit BitStream(const ByteStream& s)
      : ByteStream(s.getSubStream(s.getPosition(), s.getRemainSize())) {
    setByteOrder(Endianness::unknown);
  }

private:
  inline const uint8_t* getInput() {
    assert(data);

#if !defined(DEBUG)
    // Do we have MaxProcessBytes or more bytes left in the input buffer?
    // If so, then we can just read from said buffer.
    if (pos + BitStreamCacheBase::MaxProcessBytes <= size)
      return data + pos;
#endif

    // We have to use intermediate buffer, either because the input is running
    // out of bytes, or because we want to enforce bounds checking.

    // Note that in order to keep all fill-level invariants we must allow to
    // over-read past-the-end a bit.
    if (pos > size + BitStreamCacheBase::MaxProcessBytes)
      ThrowIOE("Buffer overflow read in BitStream");

    tmp.fill(0);

    // How many bytes are left in input buffer?
    // Since pos can be past-the-end we need to carefully handle overflow.
    Buffer::size_type bytesRemaining = (pos < size) ? size - pos : 0;
    // And if we are not at the end of the input, we may have more than we need.
    bytesRemaining =
        std::min(BitStreamCacheBase::MaxProcessBytes, bytesRemaining);

    memcpy(tmp.data(), data + pos, bytesRemaining);
    return tmp.data();
  }

public:
  inline void fill(uint32_t nbits = Cache::MaxGetBits) {
    assert(data);
    assert(nbits <= Cache::MaxGetBits);

    if (cache.fillLevel >= nbits)
      return;

    pos += fillCache(getInput(), size, &pos);
  }

  // these methods might be specialized by implementations that support it
  inline size_type getBufferPosition() const {
    return pos - (cache.fillLevel >> 3);
  }

  inline size_type getFillLevel() const { return cache.fillLevel; }

  // rewinds to the beginning of the buffer.
  void resetBufferPosition() {
    pos = 0;
    cache.fillLevel = 0;
    cache.cache = 0;
  }

  void setBufferPosition(size_type newPos);

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
