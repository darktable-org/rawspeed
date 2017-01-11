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

#include "ByteStream.h"

namespace RawSpeed {

// Note: Allocated buffer MUST be at least FILEMAP_MARGIN bytes larger than
// the compressed bit stream requires, see implementation of cache and fill()

template<typename Tag>
class BitStream
{
  typedef ByteStream::size_type size_type;
  const uchar8* data = nullptr;
  const size_type size = 0;
  size_type pos = 0; // Offset/position inside data buffer in bytes
  unsigned int bitsInCache = 0; // bits left in cache
  uint64 cache = 0;
  static constexpr unsigned MaxGetBits = 32;

  // this function will have to be specialized for each bit pump tag
  // it has to put at least MaxGetBits into the cache
  inline void fillCache();

  // these two functions have to be specialized to call either of the following two versions
  // this would ideally be implemented with partial template member function specialization if there was such a thing
  inline uint32 peekCacheBits(uint32 nbits) const;
  inline void skipCacheBits(uint32 nbits);

  inline uint32 peekCacheBitsR2L(uint32 nbits) const {
    return (cache >> (bitsInCache - nbits)) & ((1 << nbits) - 1);
  }
  inline uint32 peekCacheBitsL2R(uint32 nbits) const {
    return cache & ((1 << nbits) - 1);
  }

  inline void skipCacheBitsR2L(uint32 nbits) {
    bitsInCache -= nbits;
  }
  inline void skipCacheBitsL2R(uint32 nbits) {
    cache >>= nbits;
    bitsInCache -= nbits;
  }

public:
  BitStream(ByteStream& s)
    : data(s.peekData(s.getRemainSize())), size(s.getRemainSize() + FILEMAP_MARGIN) {}

  // deprecated:
  BitStream(FileMap* f, size_type offset, size_type size)
    : data(f->getData(offset, size)), size(size + FILEMAP_MARGIN) {}
  BitStream(FileMap* f, size_type offset)
    : BitStream(f, offset, f->getSize()-offset) {}


  inline void fill(uint32 nbits = MaxGetBits) {
    assert(nbits <= MaxGetBits);
    if (bitsInCache < nbits) {
#if 1
      // disabling this run-time bounds check saves about 1% on intel x86-64
      if (pos + MaxGetBits/8 >= size)
        ThrowIOE("Buffer overflow read in BitStream");
#endif
      fillCache();
    }
  }

  // these methods might be specialized by implementations that support it
  inline size_type getBufferPosition() const {
    return pos - (bitsInCache >> 3);
  }
  inline void setBufferPosition(size_type newPos);

  inline uint32 peekBitsNoFill(uint32 nbits) {
    assert(nbits <= MaxGetBits && nbits <= bitsInCache);
    return peekCacheBits(nbits);
  }

  inline uint32 getBitsNoFill(uint32 nbits) {
    uint32 ret = peekBitsNoFill(nbits);
    skipCacheBits(nbits);
    return ret;
  }

  inline void skipBitsNoFill(uint32 nbits) {
    assert(nbits <= MaxGetBits && nbits <= bitsInCache);
    skipCacheBits(nbits);
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
    if (nbits > bitsInCache)
      ThrowIOE("skipBits overflow");
    skipCacheBits(nbits);
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
