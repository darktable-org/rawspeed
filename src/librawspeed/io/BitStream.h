/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2017 Axel Waggershauser
    Copyright (C) 2017 Roman Lebedev

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
  uint64 cache = 0; // the acutal bits stored in the cache
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
    return cache & ((1 << count) - 1);
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
    cache = cache << count | bits;
    fillLevel += count;
  }

  inline uint32 peek(uint32 count) const noexcept {
    return (cache >> (fillLevel - count)) & ((1 << count) - 1);
  }

  inline void skip(uint32 count) noexcept {
    fillLevel -= count;
  }
};

template <typename Tag, typename Cache>
class BitStream final : public ByteStream {
  Cache cache;

  // this method hase to be implemented in the concrete BitStream template
  // specializations. It will return the number of bytes processed. It needs
  // to process up to BitStreamCacheBase::MaxProcessBytes bytes of input.
  size_type fillCache(const uchar8* input);

public:
  explicit BitStream(const ByteStream& s)
      : ByteStream(s.getSubStream(s.getPosition(), s.getRemainSize())) {}

  // deprecated:
  BitStream(const Buffer* f, size_type offset)
      : ByteStream(DataBuffer(f->getSubView(offset))) {}

private:
  inline void fillSafe() {
    assert(data);
    if (pos + BitStreamCacheBase::MaxProcessBytes <= size) {
      uchar8 tmp[BitStreamCacheBase::MaxProcessBytes] = {0};
      assert(!(size - pos < BitStreamCacheBase::MaxProcessBytes));
      memcpy(tmp, data + pos, BitStreamCacheBase::MaxProcessBytes);
      pos += fillCache(tmp);
    } else if (pos < size) {
      uchar8 tmp[BitStreamCacheBase::MaxProcessBytes] = {0};
      assert(size - pos < BitStreamCacheBase::MaxProcessBytes);
      memcpy(tmp, data + pos, size - pos);
      pos += fillCache(tmp);
    } else if (pos < size + Cache::MaxGetBits / 8) {
      // yes, this case needs to continue using Cache::MaxGetBits
      // assert(size <= pos);
      cache.push(0, Cache::MaxGetBits);
      pos += Cache::MaxGetBits / 8;
    } else {
      // assert(size < pos);
      ThrowIOE("Buffer overflow read in BitStream");
    }
  }

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
      pos += fillCache(data + pos);
#else
      // disabling this run-time bounds check saves about 1% on intel x86-64
      if (pos + BitStreamCacheBase::MaxProcessBytes <= size)
        pos += fillCache(data + pos);
      else
        fillSafe();
#endif
    }
  }

  // these methods might be specialized by implementations that support it
  inline size_type getBufferPosition() const {
    return pos - (cache.fillLevel >> 3);
  }

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
    cache.skip(nbits);
  }
};

} // namespace rawspeed
