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

#include "decompressors/KodakDecompressor.h"
#include "adt/Array2DRef.h"
#include "adt/Bit.h"
#include "adt/Casts.h"
#include "adt/Invariant.h"
#include "adt/Point.h"
#include "codes/PrefixCodeDecoder.h"
#include "common/RawImage.h"
#include "decoders/RawDecoderException.h"
#include "io/Buffer.h"
#include "io/ByteStream.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

#if __has_feature(address_sanitizer) || defined(__SANITIZE_ADDRESS__)
#include <sanitizer/asan_interface.h>
#endif

namespace rawspeed {

KodakDecompressor::KodakDecompressor(RawImage img, ByteStream bs, int bps_,
                                     bool uncorrectedRawValues_)
    : mRaw(std::move(img)), input(bs), bps(bps_),
      uncorrectedRawValues(uncorrectedRawValues_) {
  if (mRaw->getCpp() != 1 || mRaw->getDataType() != RawImageType::UINT16 ||
      mRaw->getBpp() != sizeof(uint16_t))
    ThrowRDE("Unexpected component count / data type");

  if (!mRaw->dim.hasPositiveArea() || mRaw->dim.x % 4 != 0 ||
      mRaw->dim.x > 4516 || mRaw->dim.y > 3012)
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", mRaw->dim.x,
             mRaw->dim.y);

  if (bps != 10 && bps != 12)
    ThrowRDE("Unexpected bits per sample: %i", bps);

  // Lower estimate: this decompressor requires *at least* half a byte
  // per output pixel
  (void)input.check(implicit_cast<Buffer::size_type>(mRaw->dim.area() / 2ULL));
}

KodakDecompressor::segment
KodakDecompressor::decodeSegment(const uint32_t bsize) {
  invariant(bsize > 0);
  invariant(bsize % 4 == 0);
  invariant(bsize <= segment_size);

  segment out;
  static_assert(out.size() == segment_size, "Wrong segment size");

#if __has_feature(address_sanitizer) || defined(__SANITIZE_ADDRESS__)
  // We are to produce only bsize pixels.
  __sanitizer_annotate_contiguous_container(out.begin(), out.end(), out.end(),
                                            &out[bsize]);
#endif

  std::array<uint8_t, 2 * segment_size> blen;
  uint64_t bitbuf = 0;
  uint32_t bits = 0;

  for (uint32_t i = 0; i < bsize; i += 2) {
    // One byte per two pixels
    blen[i] = input.peekByte() & 15;
    blen[i + 1] = input.getByte() >> 4;
  }
  if ((bsize & 7) == 4) {
    bitbuf = (static_cast<uint64_t>(input.getByte())) << 8UL;
    bitbuf += (static_cast<int>(input.getByte()));
    bits = 16;
  }
  for (uint32_t i = 0; i < bsize; i++) {
    uint32_t len = blen[i];
    invariant(len < 16);

    if (bits < len) {
      for (uint32_t j = 0; j < 32; j += 8) {
        bitbuf += static_cast<int64_t>(static_cast<int>(input.getByte()))
                  << (bits + (j ^ 8));
      }
      bits += 32;
    }

    uint32_t diff = static_cast<uint32_t>(bitbuf) &
                    extractHighBits(0xffffU, len, /*effectiveBitwidth=*/16);
    bitbuf >>= len;
    bits -= len;

    out[i] = implicit_cast<int16_t>(
        len != 0 ? PrefixCodeDecoder<>::extend(diff, len) : int(diff));
  }

  return out;
}

void KodakDecompressor::decompress() {
  const Array2DRef<uint16_t> out(mRaw->getU16DataAsUncroppedArray2DRef());

  uint32_t random = 0;
  for (int row = 0; row < out.height(); row++) {
    for (int col = 0; col < out.width();) {
      const int len = std::min(segment_size, mRaw->dim.x - col);

      const segment buf = decodeSegment(len);

      std::array<int, 2> pred;
      pred.fill(0);

      for (int i = 0; i < len; ++i, ++col) {
        pred[i & 1] += buf[i];

        int value = pred[i & 1];
        if (!isIntN(value, bps))
          ThrowRDE("Value out of bounds %d (bps = %i)", value, bps);

        if (uncorrectedRawValues)
          out(row, col) = implicit_cast<uint16_t>(value);
        else {
          mRaw->setWithLookUp(implicit_cast<uint16_t>(value),
                              reinterpret_cast<std::byte*>(&out(row, col)),
                              &random);
        }
      }
    }
  }
}

} // namespace rawspeed
