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

#include "common/Common.h"  // for uint32, uint64, uchar8
#include "io/Buffer.h"      // for FILEMAP_MARGIN, Buffer::size_type
#include "io/ByteStream.h"  // for ByteStream
#include "io/FileMap.h"     // for FileMap
#include "io/IOException.h" // for ThrowIOE
#include <cassert>          // for assert

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

// Note: Allocated buffer MUST be at least FILEMAP_MARGIN bytes larger than
// the compressed bit stream requires, see implementation of cache and fill()

template<typename Tag, typename Cache>
class BitStream
{
  using size_type = ByteStream::size_type;
  const uchar8* data = nullptr;
  const size_type size = 0;
  size_type pos = 0; // Offset/position inside data buffer in bytes
  Cache cache;

  // this method has to be implemented in the concrete BitStream template specializations
  void fillCache();

public:
  BitStream(ByteStream& s)
    : data(s.peekData(s.getRemainSize())), size(s.getRemainSize() + FILEMAP_MARGIN) {}

  // deprecated:
  BitStream(FileMap *f, size_type offset, size_type size_)
      : data(f->getData(offset, size_)), size(size_ + FILEMAP_MARGIN) {}
  BitStream(FileMap* f, size_type offset)
    : BitStream(f, offset, f->getSize()-offset) {}


  inline void fill(uint32 nbits = Cache::MaxGetBits) {
    assert(nbits <= Cache::MaxGetBits);
    if (cache.fillLevel < nbits) {
#if 1
      // disabling this run-time bounds check saves about 1% on intel x86-64
      if (pos + Cache::MaxGetBits/8 >= size)
        ThrowIOE("Buffer overflow read in BitStream");
#endif
      fillCache();
    }
  }

  // these methods might be specialized by implementations that support it
  inline size_type getBufferPosition() const {
    return pos - (cache.fillLevel >> 3);
  }
  inline void setBufferPosition(size_type newPos);

  inline uint32 peekBitsNoFill(uint32 nbits) {
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

  // deprecated (the above interface is already 'always save'):
  inline uint32 getBitsSafe(uint32 nbits) {
    return getBits(nbits);
  }

  inline void checkPos() const {
    if (pos >= size)
      ThrowIOE("Out of buffer read");
  }
};

} // namespace RawSpeed
