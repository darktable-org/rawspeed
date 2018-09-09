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
#include "common/Common.h"                // for uint32, ushort16, uchar8
#include "common/Point.h"                 // for iPoint2D
#include "common/RawImage.h"              // for RawImage, RawImageData
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "io/BitPumpMSB.h"                // for BitPumpMSB
#include "io/ByteStream.h"                // for ByteStream
#include <algorithm>                      // for min
#include <array>                          // for array, array<>::value_type
#include <cassert>                        // for assert
#include <cmath>                          // for abs
#include <cstdlib>                        // for abs
#include <memory>                         // for unique_ptr
#include <type_traits>                    // for enable_if_t, is_integral

namespace {

// Normally, we'd just use std::signbit(int) here. But, some (non-conforming?)
// compilers do not provide that overload, so the code simply fails to compile.
// One could cast the int to the double, but at least right now that results
// in a horrible code. So let's just provide our own signbit(). It compiles to
// the exact same code as the std::signbit(int).
template <typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
constexpr __attribute__((const)) bool SignBit(T x) {
  return x < 0;
}

} // namespace

namespace rawspeed {

OlympusDecompressor::OlympusDecompressor(const RawImage& img) : mRaw(img) {
  if (mRaw->getCpp() != 1 || mRaw->getDataType() != TYPE_USHORT16 ||
      mRaw->getBpp() != 2)
    ThrowRDE("Unexpected component count / data type");

  const uint32 w = mRaw->dim.x;
  const uint32 h = mRaw->dim.y;

  if (w == 0 || h == 0 || w % 2 != 0 || w > 10400 || h > 7792)
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", w, h);
}

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

  uchar8* data = mRaw->getData();
  int pitch = mRaw->pitch;

  /* Build a table to quickly look up "high" value */
  std::unique_ptr<char[]> bittable(new char[4096]); // NOLINT

  for (int i = 0; i < 4096; i++) {
    int b = i;
    int high;
    for (high = 0; high < 12; high++)
      if ((b >> (11 - high)) & 1)
        break;
    bittable[i] = std::min(12, high);
  }

  input.skipBytes(7);
  BitPumpMSB bits(input);

  for (uint32 y = 0; y < static_cast<uint32>(mRaw->dim.y); y++) {
    std::array<std::array<int, 3>, 2> acarry{{}};
    std::array<int, 2> left{{}};
    std::array<int, 2> nw{{}};

    auto* dest = reinterpret_cast<ushort16*>(&data[y * pitch]);
    bool y_border = y < 2;
    bool border = true;
    for (uint32 x = 0; x < static_cast<uint32>(mRaw->dim.x); x++) {
      int c = x & 1;

      bits.fill();
      int i = 2 * (acarry[c][2] < 3);
      int nbits;
      for (nbits = 2 + i; static_cast<ushort16>(acarry[c][0]) >> (nbits + i);
           nbits++)
        ;

      int b = bits.peekBitsNoFill(15);
      int sign = (b >> 14) * -1;
      int low = (b >> 12) & 3;
      int high = bittable[b & 4095];

      // Skip bytes used above or read bits
      if (high == 12) {
        bits.skipBitsNoFill(15);
        high = bits.getBits(16 - nbits) >> 1;
      } else
        bits.skipBitsNoFill(high + 1 + 3);

      acarry[c][0] = (high << nbits) | bits.getBits(nbits);
      int diff = (acarry[c][0] ^ sign) + acarry[c][1];
      acarry[c][1] = (diff * 3 + acarry[c][1]) >> 5;
      acarry[c][2] = acarry[c][0] > 16 ? 0 : acarry[c][2] + 1;

      int pred;
      if (border) {
        if (y_border && x < 2)
          pred = 0;
        else {
          if (y_border)
            pred = left[c];
          else {
            pred = dest[-pitch + (static_cast<int>(x))];
            nw[c] = pred;
          }
        }
        // Set predictor
        left[c] = pred + ((diff * 4) | low);
        // Set the pixel
        dest[x] = left[c];
      } else {
        // Have local variables for values used several tiles
        // (having a "ushort16 *dst_up" that caches dest[-pitch+((int)x)] is
        // actually slower, probably stack spill or aliasing)
        int up = dest[-pitch + (static_cast<int>(x))];
        int leftMinusNw = left[c] - nw[c];
        int upMinusNw = up - nw[c];
        // Check if sign is different, and they are both not zero
        if ((SignBit(leftMinusNw) ^ SignBit(upMinusNw)) &&
            (leftMinusNw != 0 && upMinusNw != 0)) {
          if (std::abs(leftMinusNw) > 32 || std::abs(upMinusNw) > 32)
            pred = left[c] + upMinusNw;
          else
            pred = (left[c] + up) >> 1;
        } else
          pred = std::abs(leftMinusNw) > std::abs(upMinusNw) ? left[c] : up;

        // Set predictors
        left[c] = pred + ((diff * 4) | low);
        nw[c] = up;
        // Set the pixel
        dest[x] = left[c];
      }

      if (c)
        border = y_border;
    }
  }
}

} // namespace rawspeed
