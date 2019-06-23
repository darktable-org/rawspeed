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

#include "common/Common.h" // for uint32, uchar8, uint64
#include "io/Buffer.h"     // for Buffer::size_type, BUFFER_PADDING
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
  uint64 cache = 0; // the actual bits stored in the cache
  unsigned int fillLevel = 0; // bits left in cache
  static constexpr unsigned Size = sizeof(cache)*8;

  // how many bits could be requested to be filled
  static constexpr unsigned MaxGetBits = Size/2;

  // maximal number of bytes the implementation may read.
  // NOTE: this is not the same as MaxGetBits/8 !!!
  static constexpr unsigned MaxProcessBytes = 8;
};

struct BitStreamCacheLeftInRightOut : BitStreamCacheBase
{
  inline void push(uint64 bits, uint32 count) noexcept {
    assert(count + fillLevel <= Size);
    cache |= bits << fillLevel;
    fillLevel += count;
  }

  inline uint32 peek(uint32 count) const noexcept {
    return cache & ((1U << count) - 1U);
  }

  inline void skip(uint32 count) noexcept {
    cache >>= count;
    fillLevel -= count;
  }
};

struct BitStreamCacheRightInLeftOut : BitStreamCacheBase
{
  inline void push(uint64 bits, uint32 count) noexcept {
    assert(count + fillLevel <= Size);
    assert(count < BitStreamCacheBase::Size);
    cache = cache << count | bits;
    fillLevel += count;
  }

  inline uint32 peek(uint32 count) const noexcept {
    return (cache >> (fillLevel - count)) & ((1U << count) - 1U);
  }

  inline void skip(uint32 count) noexcept {
    fillLevel -= count;
  }
};

template <typename BIT_STREAM> struct BitStreamTraits final {
  static constexpr bool canUseWithHuffmanTable = false;
};

template <typename Tag, typename Cache>
class BitStream final : public ByteStream {
  Cache cache;

  // this method hase to be implemented in the concrete BitStream template
  // specializations. It will return the number of bytes processed. It needs
  // to process up to BitStreamCacheBase::MaxProcessBytes bytes of input.
  size_type fillCache(const uchar8* input, size_type bufferSize,
                      size_type* bufPos);

public:
  BitStream() = default;

  explicit BitStream(const ByteStream& s)
      : ByteStream(s.getSubStream(s.getPosition(), s.getRemainSize())) {
    setByteOrder(Endianness::unknown);
  }

private:
  inline void fillSafe() {
    assert(data);
    if (pos + BitStreamCacheBase::MaxProcessBytes <= size) {
      std::array<uchar8, BitStreamCacheBase::MaxProcessBytes> tmp;
      tmp.fill(0);
      assert(!(size - pos < BitStreamCacheBase::MaxProcessBytes));
      memcpy(tmp.data(), data + pos, BitStreamCacheBase::MaxProcessBytes);
      pos += fillCache(tmp.data(), size, &pos);
    } else if (pos < size) {
      std::array<uchar8, BitStreamCacheBase::MaxProcessBytes> tmp;
      tmp.fill(0);
      assert(size - pos < BitStreamCacheBase::MaxProcessBytes);
      memcpy(tmp.data(), data + pos, size - pos);
      pos += fillCache(tmp.data(), size, &pos);
    } else if (pos <= size + BitStreamCacheBase::MaxProcessBytes) {
      std::array<uchar8, BitStreamCacheBase::MaxProcessBytes> tmp;
      tmp.fill(0);
      pos += fillCache(tmp.data(), size, &pos);
    } else {
      // assert(size < pos);
      ThrowIOE("Buffer overflow read in BitStream");
    }
  }

  // In non-DEBUG builds, fillSafe() will be called at most once
  // per the life-time of the BitStream  therefore it should *NOT* be inlined
  // into the normal codepath.
  inline void __attribute__((noinline, cold)) fillSafeNoinline() { fillSafe(); }

public:
  inline void fill(uint32 nbits = Cache::MaxGetBits) {
    assert(data);
    assert(nbits <= Cache::MaxGetBits);
    if (cache.fillLevel < nbits) {
#if defined(DEBUG)
      // really slow, but best way to check all the assumptions.
      fillSafe();
#elif BUFFER_PADDING >= 8
      static_assert(BitStreamCacheBase::MaxProcessBytes == 8,
                    "update these too");
      // FIXME: this looks very wrong. We don't check pos at all here.
      // I suspect this should be:  if (pos <= size)
      pos += fillCache(data + pos, size, &pos);
#else
      // disabling this run-time bounds check saves about 1% on intel x86-64
      if (pos + BitStreamCacheBase::MaxProcessBytes <= size)
        pos += fillCache(data + pos, size, &pos);
      else
        fillSafeNoinline();
#endif
    }
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

  inline uint32 __attribute__((pure)) peekBitsNoFill(uint32 nbits) {
    assert(nbits <= Cache::MaxGetBits);
    assert(nbits <= cache.fillLevel);
    return cache.peek(nbits);
  }

  inline uint32 getBitsNoFill(uint32 nbits) {
    uint32 ret = peekBitsNoFill(nbits);
    cache.skip(nbits);
    return ret;
  }

  inline void skipBitsNoFill(uint32 nbits) {
    assert(nbits <= Cache::MaxGetBits);
    assert(nbits <= cache.fillLevel);
    cache.skip(nbits);
  }

  inline uint32 peekBits(uint32 nbits) {
    fill(nbits);
    return peekBitsNoFill(nbits);
  }

  inline uint32 getBits(uint32 nbits) {
    fill(nbits);
    return getBitsNoFill(nbits);
  }

  inline void skipBits(uint32 nbits) {
    if (nbits > cache.fillLevel)
      ThrowIOE("skipBits overflow");
    skipBitsNoFill(nbits);
  }

  // This may be used to skip arbitrarily large number of *bytes*,
  // not limited by the fill level.
  inline void skipBytes(uint32 nbytes) {
    uint32 remainingBitsToSkip = 8 * nbytes;
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
