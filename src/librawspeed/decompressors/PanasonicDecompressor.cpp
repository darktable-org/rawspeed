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
#include "common/Mutex.h"    // for MutexLocker
#include "common/Point.h"    // for iPoint2D
#include "common/RawImage.h" // for RawImage, RawImageData
#include <algorithm>         // for min, move
#include <cstring>           // for memcpy
#include <vector>            // for vector

namespace rawspeed {

PanasonicDecompressor::PanasonicDecompressor(const RawImage& img,
                                             const ByteStream& input_,
                                             bool zero_is_not_bad,
                                             uint32 load_flags_)
    : AbstractParallelizedDecompressor(img), zero_is_bad(!zero_is_not_bad),
      load_flags(load_flags_) {
  if (mRaw->getCpp() != 1 || mRaw->getDataType() != TYPE_USHORT16 ||
      mRaw->getBpp() != 2)
    ThrowRDE("Unexpected component count / data type");

  const uint32 width = mRaw->dim.x;
  const uint32 height = mRaw->dim.y;

  if (width == 0 || height == 0 || width % 14 != 0)
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", width, height);

  /*
   * Normally, we would check the image dimensions against some hardcoded
   * threshold. That is being done as poor man's attempt to catch
   * obviously-invalid raws, and avoid OOM's during fuzzing. However, there is
   * a better solution - actually check the size of input buffer to try and
   * guess whether the image size is valid or not. And in this case, we can do
   * that, because the compression rate is static and known.
   */
  // if (width > 5488 || height > 3912)
  //   ThrowRDE("Too large image size: (%u; %u)", width, height);

  if (BufSize < load_flags)
    ThrowRDE("Bad load_flags: %u, less than BufSize (%u)", load_flags, BufSize);

  // Naive count of bytes that given pixel count requires.
  const auto rawBytesNormal = 8U * mRaw->dim.area() / 7U;
  // If load_flags is zero, than that size is the size we need to read.
  // But if it is not, then we need to round up to multiple of BufSize, because
  // of splitting&rotation of each BufSize's slice in half at load_flags bytes.
  const auto bufSize =
      load_flags == 0 ? rawBytesNormal : roundUp(rawBytesNormal, BufSize);
  input = input_.peekStream(bufSize);
}

struct PanasonicDecompressor::PanaBitpump {
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
      assert(BufSize >= load_flags);
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

  assert(mRaw->dim.x % 14 == 0);
  const auto blocks = mRaw->dim.x / 14;

  std::vector<uint32> zero_pos;
  for (uint32 y = t->start; y < t->end; y++) {
    int sh = 0;

    auto* dest = reinterpret_cast<ushort16*>(mRaw->getData(0, y));
    for (int block = 0; block < blocks; block++) {
      std::array<int, 2> pred;
      pred.fill(0);

      std::array<int, 2> nonz;
      nonz.fill(0);

      int u = 0;

      for (int x = 0; x < 14; x++) {
        const int c = x & 1;

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
          if (nonz[c] || x > 11)
            pred[c] = nonz[c] << 4 | bits.getBits(4);
        }

        *dest = pred[c];

        if (zero_is_bad && 0 == pred[c])
          zero_pos.push_back((y << 16) | (14 * block + x));

        u++;
        dest++;
      }
    }
  }

  if (zero_is_bad && !zero_pos.empty()) {
    MutexLocker guard(&mRaw->mBadPixelMutex);
    mRaw->mBadPixelPositions.insert(mRaw->mBadPixelPositions.end(),
                                    zero_pos.begin(), zero_pos.end());
  }
}

} // namespace rawspeed
