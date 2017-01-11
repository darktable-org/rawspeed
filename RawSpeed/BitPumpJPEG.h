/*
    RawSpeed - RAW file decoder.

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

#include "BitStream.h"

namespace RawSpeed {

struct JPEGBitPumpTag;

// The JPEG data is ordered in MSB bit order,
// i.e. we push into the cache from the right and read it from the left

template<> inline void BitStream<JPEGBitPumpTag>::fillCache() {
  static_assert(sizeof(cache) == 8 && MaxGetBits == 32, "if the structure of the bit cache changed, this code has to be updated");

  const uint32 nBytes = 4;
  // short-cut path for the most common case (no FF marker in the next 4 bytes)
  // this is slightly faster than the else-case alone.
  // TODO: investigate applicability of vector intrinsics to speed up if-cascade
  if (data[pos+0] != 0xFF &&
      data[pos+1] != 0xFF &&
      data[pos+2] != 0xFF &&
      data[pos+3] != 0xFF ) {
    cache = cache << nBytes*8 | loadMem<uint32>(data+pos, getHostEndianness() == little);
    pos += nBytes;
    bitsInCache += nBytes*8;
  } else {
    for (uint32 i = 0; i < nBytes; ++i) {
      // Pre-execute most common case, where next byte is 'normal'/non-FF
      const int c0 = data[pos++];
      cache = (cache << 8) | c0;
      bitsInCache += 8;
      if (c0 == 0xFF) {
        // Found FF -> pre-execute case of FF/00, which represents an FF data byte -> ignore the 00
        const int c1 = data[pos++];
        if (c1 != 0) {
          // Found FF/xx with xx != 00. This is the end of stream marker.
          // Rewind pos to the FF byte, in case we get called again.
          // Fill the cache with zeros and keep on doing that from now on.
          pos -= 2;
          cache &= ~0xFF;
          cache <<= 64 - bitsInCache;
          bitsInCache = 64;
          break;
        }
      }
    }
  }
}

template<> inline uint32 BitStream<JPEGBitPumpTag>::peekCacheBits(uint32 nbits) const {
  return peekCacheBitsR2L(nbits);
}

template<> inline void BitStream<JPEGBitPumpTag>::skipCacheBits(uint32 nbits) {
  skipCacheBitsR2L(nbits);
}

template<> inline BitStream<JPEGBitPumpTag>::size_type BitStream<JPEGBitPumpTag>::getBufferPosition() const {
  return pos; // the current number byte we consumed -> at the end of the stream pos points to the JPEG marker FF
}

using BitPumpJPEG = BitStream<JPEGBitPumpTag>;

} // namespace RawSpeed
