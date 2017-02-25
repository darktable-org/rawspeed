/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
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

#include "common/Common.h" // for uint32, uchar8, uint64
#include "io/Buffer.h"     // for Buffer::size_type, BUFFER_PADDING
#include "io/ByteStream.h"  // for ByteStream
#include "io/IOException.h" // for IOException (ptr only), ThrowIOE
#include <cassert>          // for assert
#include <cstring>          // for memcpy

namespace RawSpeed {

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
  static constexpr unsigned MaxGetBits = Size/2;
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

template<typename Tag, typename Cache>
class BitStream : private ByteStream
{
  Cache cache;

  // this method hase to be implemented in the concrete BitStream template
  // specializations. It needs to process up to 4 bytes of input and return
  // the number of bytes processed
  size_type fillCache(const uchar8* input);

public:
  BitStream(ByteStream& s)
      : ByteStream(s.getSubStream(s.getPosition(), s.getRemainSize())) {}

  // deprecated:
  BitStream(Buffer* f, size_type offset) : ByteStream(f->getSubView(offset)) {}

  inline void fill(uint32 nbits = Cache::MaxGetBits) {
    assert(nbits <= Cache::MaxGetBits);
    if (cache.fillLevel < nbits) {
#if BUFFER_PADDING==0
      // disabling this run-time bounds check saves about 1% on intel x86-64
      if (pos + Cache::MaxGetBits/8 > size) {
        if (pos < size) {
          uchar8 tmp[4] = {0, 0, 0, 0};
          memcpy(tmp, data + pos, size - pos);
          pos += fillCache(tmp);
        } else if (pos < size + 4) {
          cache.push(0, Cache::MaxGetBits);
          pos += Cache::MaxGetBits/8;
        } else
          ThrowIOE("Buffer overflow read in BitStream");
      } else
#endif
        pos += fillCache(data + pos);
    }
  }

  // these methods might be specialized by implementations that support it
  inline size_type getBufferPosition() const {
    return pos - (cache.fillLevel >> 3);
  }
  inline void setBufferPosition(size_type newPos);

  inline uint32 __attribute__((pure)) peekBitsNoFill(uint32 nbits) {
    assert(nbits <= Cache::MaxGetBits && nbits <= cache.fillLevel);
    return cache.peek(nbits);
  }

  inline uint32 getBitsNoFill(uint32 nbits) {
    uint32 ret = peekBitsNoFill(nbits);
    cache.skip(nbits);
    return ret;
  }

  inline void skipBitsNoFill(uint32 nbits) {
    assert(nbits <= Cache::MaxGetBits && nbits <= cache.fillLevel);
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

} // namespace RawSpeed
