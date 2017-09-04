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

#include "decompressors/PanasonicDecompressor.h"
#include <vector> // for vector

namespace rawspeed {

PanasonicDecompressor::PanasonicDecompressor(const RawImage& img,
                                             ByteStream input_,
                                             bool zero_is_not_bad,
                                             uint32 load_flags_)
    : AbstractParallelizedDecompressor(img), input(std::move(input_)),
      zero_is_bad(!zero_is_not_bad), load_flags(load_flags_) {}

struct PanasonicDecompressor::PanaBitpump {
  static constexpr uint32 BufSize = 0x4000;
  ByteStream input;
  std::vector<uchar8> buf;
  int vbits = 0;
  uint32 load_flags;

  PanaBitpump(ByteStream input_, int load_flags_)
      : input(std::move(input_)), load_flags(load_flags_) {
    // get one more byte, so the return statement of getBits does not have
    // to special case for accessing the last byte
    buf.resize(BufSize + 1UL);
  }

  void skipBytes(int bytes) {
    int blocks = (bytes / BufSize) * BufSize;
    input.skipBytes(blocks);
    for (int i = blocks; i < bytes; i++)
      (void)getBits(8);
  }

  uint32 getBits(int nbits) {
    if (!vbits) {
      /* On truncated files this routine will just return just for the truncated
       * part of the file. Since there is no chance of affecting output buffer
       * size we allow the decoder to decode this
       */
      auto size = std::min(input.getRemainSize(), BufSize - load_flags);
      memcpy(buf.data() + load_flags, input.getData(size), size);

      size = std::min(input.getRemainSize(), load_flags);
      if (size != 0)
        memcpy(buf.data(), input.getData(size), size);
    }
    vbits = (vbits - nbits) & 0x1ffff;
    int byte = vbits >> 3 ^ 0x3ff0;
    return (buf[byte] | buf[byte + 1UL] << 8) >> (vbits & 7) & ~(-(1 << nbits));
  }
};

void PanasonicDecompressor::decompressThreaded(
    const RawDecompressorThread* t) const {
  PanaBitpump bits(input, load_flags);

  /* 9 + 1/7 bits per pixel */
  bits.skipBytes(8 * mRaw->dim.x * t->start / 7);

  std::vector<uint32> zero_pos;
  for (uint32 y = t->start; y < t->end; y++) {
    int sh = 0;
    int pred[2];
    int nonz[2];
    int u = 0;

    auto* dest = reinterpret_cast<ushort16*>(mRaw->getData(0, y));
    for (int x = 0; x < mRaw->dim.x; x++) {
      const int i = x % 14;
      const int c = x & 1;

      // did we process one whole block of 14 pixels?
      if (i == 0)
        u = pred[0] = pred[1] = nonz[0] = nonz[1] = 0;

      if (u == 2) {
        sh = 4 >> (3 - bits.getBits(2));
        u = -1;
      }

      if (nonz[c]) {
        int j = bits.getBits(8);
        if (j) {
          pred[c] -= 0x80 << sh;
          if (pred[c] < 0 || sh == 4)
            pred[c] &= ~(-(1 << sh));
          pred[c] += j << sh;
        }
      } else {
        nonz[c] = bits.getBits(8);
        if (nonz[c] || i > 11)
          pred[c] = nonz[c] << 4 | bits.getBits(4);
      }

      *dest = pred[c];

      if (zero_is_bad && 0 == pred[c])
        zero_pos.push_back((y << 16) | x);

      u++;
      dest++;
    }
  }

  if (zero_is_bad && !zero_pos.empty()) {
    MutexLocker guard(&mRaw->mBadPixelMutex);
    mRaw->mBadPixelPositions.insert(mRaw->mBadPixelPositions.end(),
                                    zero_pos.begin(), zero_pos.end());
  }
}

} // namespace rawspeed
