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
#include "common/Mutex.h"                 // for MutexLocker
#include "common/Point.h"                 // for iPoint2D
#include "common/RawImage.h"              // for RawImage, RawImageData
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include <algorithm>                      // for min
#include <array>                          // for array
#include <cassert>                        // for assert
#include <cstring>                        // for memcpy
#include <utility>                        // for move
#include <vector>                         // for vector

namespace rawspeed {

PanasonicDecompressor::PanasonicDecompressor(const RawImage& img,
                                             const ByteStream& input_,
                                             bool zero_is_not_bad,
                                             uint32 section_split_offset_)
    : AbstractParallelizedDecompressor(img), zero_is_bad(!zero_is_not_bad),
      section_split_offset(section_split_offset_) {
  if (mRaw->getCpp() != 1 || mRaw->getDataType() != TYPE_USHORT16 ||
      mRaw->getBpp() != 2)
    ThrowRDE("Unexpected component count / data type");

  if (!mRaw->dim.hasPositiveArea() || mRaw->dim.x % 14 != 0) {
    ThrowRDE("Unexpected image dimensions found: (%i; %i)", mRaw->dim.x,
             mRaw->dim.y);
  }

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

  if (BufSize < section_split_offset)
    ThrowRDE("Bad section_split_offset: %u, less than BufSize (%u)", section_split_offset, BufSize);

  // Naive count of bytes that given pixel count requires.
  // Do division first, because we know the remainder is always zero,
  // and the next multiplication won't overflow.
  assert(mRaw->dim.area() % 7ULL == 0ULL);
  const auto rawBytesNormal = (mRaw->dim.area() / 7ULL) * 8ULL;
  // If section_split_offset is zero, then that we need to read the normal amount of bytes.
  // But if it is not, then we need to round up to multiple of BufSize, because
  // of splitting&rotation of each BufSize's slice in half at section_split_offset bytes.
  const auto bufSize =
      section_split_offset == 0 ? rawBytesNormal : roundUp(rawBytesNormal, BufSize);
  input = input_.peekStream(bufSize);
}

struct PanasonicDecompressor::PanaBitpump {
  ByteStream input;
  std::vector<uchar8> buf;
  int vbits = 0;
  uint32 section_split_offset;

  PanaBitpump(ByteStream input_, int section_split_offset_)
      : input(std::move(input_)), section_split_offset(section_split_offset_) {
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
      /* On truncated files this routine will just return for the truncated
       * part of the file. Since there is no chance of affecting output buffer
       * size we allow the decoder to decode this
       */
      assert(BufSize >= section_split_offset);
      auto size = std::min(input.getRemainSize(), BufSize - section_split_offset);
      memcpy(buf.data() + section_split_offset, input.getData(size), size);

      size = std::min(input.getRemainSize(), section_split_offset);
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
  PanaBitpump bits(input, section_split_offset);

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
