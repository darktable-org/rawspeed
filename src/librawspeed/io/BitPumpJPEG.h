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

#include "common/Common.h" // for bitwidth
#include "io/BitStream.h"  // for BitStreamCacheRightInLeftOut, BitStream
#include "io/Buffer.h"     // for Buffer::size_type
#include "io/Endianness.h" // for getBE
#include <cstdint>         // for uint8_t, uint32_t, uint64_t

namespace rawspeed {

struct JPEGBitPumpTag;

// The JPEG data is ordered in MSB bit order,
// i.e. we push into the cache from the right and read it from the left
using BitPumpJPEG = BitStream<JPEGBitPumpTag, BitStreamCacheRightInLeftOut>;

template <> struct BitStreamTraits<JPEGBitPumpTag> final {
  static constexpr bool canUseWithHuffmanTable = true;

  // How many bytes can we read from the input per each fillCache(), at most?
  // Normally, we want to read 4 bytes, but at worst each one of those can be
  // an 0xFF byte, separated by 0x00 byte, signifying that 0xFF is a data byte.
  static constexpr int MaxProcessBytes = 8;
};

template <>
inline BitPumpJPEG::size_type BitPumpJPEG::fillCache(const uint8_t* input) {
  static_assert(BitStreamCacheBase::MaxGetBits >= 32, "check implementation");

  std::array<uint8_t, BitStreamTraits<JPEGBitPumpTag>::MaxProcessBytes>
      prefetch;
  std::copy_n(input, prefetch.size(), prefetch.begin());

  // short-cut path for the most common case (no FF marker in the next 4 bytes)
  // this is slightly faster than the else-case alone.
  if (std::none_of(&prefetch[0], &prefetch[4],
                   [](uint8_t byte) { return byte == 0xFF; })) {
    cache.push(getBE<uint32_t>(prefetch.data()), 32);
    return 4;
  }

  size_type p = 0;
  for (size_type i = 0; i < 4; ++i) {
    // Pre-execute most common case, where next byte is 'normal'/non-FF
    const int c0 = prefetch[p];
    ++p;
    cache.push(c0, 8);
    if (c0 == 0xFF) {
      // Found FF -> pre-execute case of FF/00, which represents an FF data byte -> ignore the 00
      const int c1 = prefetch[p];
      ++p;
      if (c1 != 0) {
        // Found FF/xx with xx != 00. This is the end of stream marker.
        // That means we shouldn't have pushed last 8 bits (0xFF, from c0).
        // We need to "unpush" them, and fill the vacant cache bits with zeros.

        // First, recover the cache fill level.
        cache.fillLevel -= 8;
        // Now, this code is incredibly underencapsulated, and the
        // implementation details are leaking into here. Thus, we know that
        // all the fillLevel bits in cache are all high bits. So to "unpush"
        // the last 8 bits, and fill the vacant cache bits with zeros, we only
        // need to keep the high fillLevel bits. So just create a mask with only
        // high fillLevel bits set, and 'and' the cache with it.
        // Caution, we know fillLevel won't be 64, but it may be 0,
        // so pick the mask-creation idiom accordingly.
        cache.cache &= ~((~0ULL) >> cache.fillLevel);
        cache.fillLevel = 64;

        // No further reading from this buffer shall happen. Do signal that by
        // claiming that we have consumed all the remaining bytes of the buffer.
        return getRemainingSize();
      }
    }
  }
  return p;
}

template <>
inline BitPumpJPEG::size_type BitPumpJPEG::getStreamPosition() const {
  // the current number of bytes we consumed -> at the end of the stream pos, it
  // points to the JPEG marker FF
  return getInputPosition();
}

} // namespace rawspeed
