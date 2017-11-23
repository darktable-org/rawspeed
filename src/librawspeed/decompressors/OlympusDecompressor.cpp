/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2014-2015 Pedro Côrte-Real
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

#include "decompressors/OlympusDecompressor.h"
#include "common/Common.h"                      // for uchar8
#include "common/Point.h"                       // for iPoint2D
#include "common/RawImage.h"                    // for RawImage
#include "decoders/RawDecoderException.h"       // for ThrowRDE
#include "decompressors/AbstractDecompressor.h" // for RawDecom...
#include "decompressors/HuffmanTable.h"         // for HuffmanTable
#include "io/BitPumpMSB.h"                      // for BitPumpMSB
#include <algorithm>                            // for move
#include <algorithm>                            // for min
#include <cmath>                                // for signbit
#include <memory>                               // for unique_ptr

namespace rawspeed {

OlympusDecompressor::OlympusDecompressor(const RawImage& img) : mRaw(img) {}

/* This is probably the slowest decoder of them all.
 * I cannot see any way to effectively speed up the prediction
 * phase, which is by far the slowest part of this algorithm.
 * Also there is no way to multithread this code, since prediction
 * is based on the output of all previous pixel (bar the first four)
 */

void OlympusDecompressor::decompress(ByteStream input) const {
  assert(mRaw->dim.y > 0);
  assert(mRaw->dim.x > 0);
  assert(mRaw->dim.x % 2 == 0);

  int nbits;
  int sign;
  int low;
  int high;
  int i;
  int left0 = 0;
  int nw0 = 0;
  int left1 = 0;
  int nw1 = 0;
  int pred;
  int diff;

  uchar8* data = mRaw->getData();
  int pitch = mRaw->pitch;

  /* Build a table to quickly look up "high" value */
  std::unique_ptr<char[]> bittable(new char[4096]);

  for (i = 0; i < 4096; i++) {
    int b = i;
    for (high = 0; high < 12; high++)
      if ((b >> (11 - high)) & 1)
        break;
    bittable[i] = std::min(12, high);
  }

  input.skipBytes(7);
  BitPumpMSB bits(input);

  for (uint32 y = 0; y < static_cast<uint32>(mRaw->dim.y); y++) {
    int acarry0[3] = {};
    int acarry1[3] = {};

    auto* dest = reinterpret_cast<ushort16*>(&data[y * pitch]);
    bool y_border = y < 2;
    bool border = true;
    for (uint32 x = 0; x < static_cast<uint32>(mRaw->dim.x); x++) {
      bits.fill();
      i = 2 * (acarry0[2] < 3);
      for (nbits = 2 + i; static_cast<ushort16>(acarry0[0]) >> (nbits + i);
           nbits++)
        ;

      int b = bits.peekBitsNoFill(15);
      sign = (b >> 14) * -1;
      low = (b >> 12) & 3;
      high = bittable[b & 4095];

      // Skip bytes used above or read bits
      if (high == 12) {
        bits.skipBitsNoFill(15);
        high = bits.getBits(16 - nbits) >> 1;
      } else
        bits.skipBitsNoFill(high + 1 + 3);

      acarry0[0] = (high << nbits) | bits.getBits(nbits);
      diff = (acarry0[0] ^ sign) + acarry0[1];
      acarry0[1] = (diff * 3 + acarry0[1]) >> 5;
      acarry0[2] = acarry0[0] > 16 ? 0 : acarry0[2] + 1;

      if (border) {
        if (y_border && x < 2)
          pred = 0;
        else {
          if (y_border)
            pred = left0;
          else {
            pred = dest[-pitch + (static_cast<int>(x))];
            nw0 = pred;
          }
        }
        dest[x] = pred + ((diff * 4) | low);
        // Set predictor
        left0 = dest[x];
      } else {
        // Have local variables for values used several tiles
        // (having a "ushort16 *dst_up" that caches dest[-pitch+((int)x)] is
        // actually slower, probably stack spill or aliasing)
        int up = dest[-pitch + (static_cast<int>(x))];
        int leftMinusNw = left0 - nw0;
        int upMinusNw = up - nw0;
        // Check if sign is different, and they are both not zero
        if ((std::signbit(leftMinusNw) ^ std::signbit(upMinusNw)) &&
            (leftMinusNw != 0 && upMinusNw != 0)) {
          if (std::abs(leftMinusNw) > 32 || std::abs(upMinusNw) > 32)
            pred = left0 + upMinusNw;
          else
            pred = (left0 + up) >> 1;
        } else
          pred = std::abs(leftMinusNw) > std::abs(upMinusNw) ? left0 : up;

        dest[x] = pred + ((diff * 4) | low);
        // Set predictors
        left0 = dest[x];
        nw0 = up;
      }

      // ODD PIXELS
      x += 1;
      bits.fill();
      i = 2 * (acarry1[2] < 3);
      for (nbits = 2 + i; static_cast<ushort16>(acarry1[0]) >> (nbits + i);
           nbits++)
        ;
      b = bits.peekBitsNoFill(15);
      sign = (b >> 14) * -1;
      low = (b >> 12) & 3;
      high = bittable[b & 4095];

      // Skip bytes used above or read bits
      if (high == 12) {
        bits.skipBitsNoFill(15);
        high = bits.getBits(16 - nbits) >> 1;
      } else
        bits.skipBitsNoFill(high + 1 + 3);

      acarry1[0] = (high << nbits) | bits.getBits(nbits);
      diff = (acarry1[0] ^ sign) + acarry1[1];
      acarry1[1] = (diff * 3 + acarry1[1]) >> 5;
      acarry1[2] = acarry1[0] > 16 ? 0 : acarry1[2] + 1;

      if (border) {
        if (y_border && x < 2)
          pred = 0;
        else {
          if (y_border)
            pred = left1;
          else {
            pred = dest[-pitch + (static_cast<int>(x))];
            nw1 = pred;
          }
        }
        dest[x] = left1 = pred + ((diff * 4) | low);
      } else {
        int up = dest[-pitch + (static_cast<int>(x))];
        int leftMinusNw = left1 - nw1;
        int upMinusNw = up - nw1;

        // Check if sign is different, and they are both not zero
        if ((std::signbit(leftMinusNw) ^ std::signbit(upMinusNw)) &&
            (leftMinusNw != 0 && upMinusNw != 0)) {
          if (std::abs(leftMinusNw) > 32 || std::abs(upMinusNw) > 32)
            pred = left1 + upMinusNw;
          else
            pred = (left1 + up) >> 1;
        } else
          pred = std::abs(leftMinusNw) > std::abs(upMinusNw) ? left1 : up;

        dest[x] = left1 = pred + ((diff * 4) | low);
        nw1 = up;
      }
      border = y_border;
    }
  }
}

} // namespace rawspeed
