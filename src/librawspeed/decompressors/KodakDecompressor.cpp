/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2014 Pedro Côrte-Real
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

#include "rawspeedconfig.h"
#include "decompressors/KodakDecompressor.h"
#include "common/RawImage.h"              // for RawImage
#include "decoders/RawDecoderException.h" // for RawDecoderException (ptr o...
#include "decompressors/HuffmanTable.h"   // for HuffmanTable
#include "io/ByteStream.h"                // for ByteStream
#include <algorithm>                      // for min
#include <array>                          // for array
#include <cassert>                        // for assert

namespace rawspeed {

constexpr int KodakDecompressor::segment_size;

KodakDecompressor::KodakDecompressor(const RawImage& img, ByteStream bs,
                                     bool uncorrectedRawValues_)
    : mRaw(img), input(std::move(bs)),
      uncorrectedRawValues(uncorrectedRawValues_) {
  if (mRaw->getCpp() != 1 || mRaw->getDataType() != TYPE_USHORT16 ||
      mRaw->getBpp() != 2)
    ThrowRDE("Unexpected component count / data type");

  if (mRaw->dim.x == 0 || mRaw->dim.y == 0 || mRaw->dim.x % 4 != 0 ||
      mRaw->dim.x > 4516 || mRaw->dim.y > 3012)
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", mRaw->dim.x,
             mRaw->dim.y);

  // Lower estimate: this decompressor requires *at least* half a byte
  // per output pixel
  input.check(mRaw->dim.area() / 2ULL);
}

KodakDecompressor::segment
KodakDecompressor::decodeSegment(const uint32 bsize) {
  assert(bsize > 0);
  assert(bsize % 4 == 0);
  assert(bsize <= segment_size);

  segment out;
  static_assert(out.size() == segment_size, "Wrong segment size");

#if __has_feature(address_sanitizer) || defined(__SANITIZE_ADDRESS__)
  // We are to produce only bsize pixels.
  __sanitizer_annotate_contiguous_container(out.begin(), out.end(), out.end(),
                                            out.begin() + bsize);
#endif

  std::array<uchar8, 2 * segment_size> blen;
  uint64 bitbuf = 0;
  uint32 bits = 0;

  for (uint32 i = 0; i < bsize; i += 2) {
    // One byte per two pixels
    blen[i] = input.peekByte() & 15;
    blen[i + 1] = input.getByte() >> 4;
  }
  if ((bsize & 7) == 4) {
    bitbuf = (static_cast<uint64>(input.getByte())) << 8UL;
    bitbuf += (static_cast<int>(input.getByte()));
    bits = 16;
  }
  for (uint32 i = 0; i < bsize; i++) {
    uint32 len = blen[i];

    if (bits < len) {
      for (uint32 j = 0; j < 32; j += 8) {
        bitbuf += static_cast<long long>(static_cast<int>(input.getByte()))
                  << (bits + (j ^ 8));
      }
      bits += 32;
    }

    uint32 diff = static_cast<uint32>(bitbuf) & (0xffff >> (16 - len));
    bitbuf >>= len;
    bits -= len;
    diff = len != 0 ? HuffmanTable::signExtended(diff, len) : diff;

    out[i] = diff;
  }

  return out;
}

void KodakDecompressor::decompress() {
  uchar8* data = mRaw->getData();
  uint32 pitch = mRaw->pitch;

  uint32 random = 0;
  for (auto y = 0; y < mRaw->dim.y; y++) {
    auto* dest = reinterpret_cast<ushort16*>(&data[y * pitch]);

    for (auto x = 0; x < mRaw->dim.x; x += segment_size) {
      const uint32 len = std::min(segment_size, mRaw->dim.x - x);

      const segment buf = decodeSegment(len);

      std::array<uint32, 2> pred;
      pred.fill(0);

      for (uint32 i = 0; i < len; i++) {
        pred[i & 1] += buf[i];

        ushort16 value = pred[i & 1];
        if (value > 1023)
          ThrowRDE("Value out of bounds %d", value);

        if (uncorrectedRawValues)
          dest[x + i] = value;
        else
          mRaw->setWithLookUp(value, reinterpret_cast<uchar8*>(&dest[x + i]),
                              &random);
      }
    }
  }
};

} // namespace rawspeed
