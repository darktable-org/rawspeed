/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2014-2015 Pedro Côrte-Real
    Copyright (C) 2017-2018 Roman Lebedev

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
#include "common/Array2DRef.h"            // for Array2DRef
#include "common/Point.h"                 // for iPoint2D
#include "common/RawImage.h"              // for RawImage, RawImageData
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "io/BitPumpMSB.h"                // for BitPumpMSB
#include "io/ByteStream.h"                // for ByteStream
#include <array>                          // for array
#include <cassert>                        // for assert
#include <cstdlib>                        // for abs
#include <type_traits>                    // for enable_if_t, is_integral

namespace {

// Normally, we'd just use std::signbit(int) here. But, some (non-conforming?)
// compilers do not provide that overload, so the code simply fails to compile.
// One could cast the int to the double, but at least right now that results
// in a horrible code. So let's just provide our own signbit(). It compiles to
// the exact same code as the std::signbit(int).
template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
constexpr __attribute__((const)) bool SignBit(T x) {
  return x < 0;
}

} // namespace

namespace rawspeed {

OlympusDecompressor::OlympusDecompressor(RawImageData *img) : mRaw(img) {
  if (mRaw->getCpp() != 1 || mRaw->getDataType() != RawImageType::UINT16 ||
      mRaw->getBpp() != sizeof(uint16_t))
    ThrowRDE("Unexpected component count / data type");

  const uint32_t w = mRaw->dim.x;
  const uint32_t h = mRaw->dim.y;

  if (w == 0 || h == 0 || w % 2 != 0 || w > 10400 || h > 7792)
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", w, h);
}

/* This is probably the slowest decoder of them all.
 * I cannot see any way to effectively speed up the prediction
 * phase, which is by far the slowest part of this algorithm.
 * Also there is no way to multithread this code, since prediction
 * is based on the output of all previous pixel (bar the first four)
 */

inline __attribute__((always_inline)) int
OlympusDecompressor::parseCarry(BitPumpMSB& bits,
                                std::array<int, 3>* carry) const {
  bits.fill();
  int i = 2 * ((*carry)[2] < 3);
  int nbits;
  for (nbits = 2 + i; static_cast<uint16_t>((*carry)[0]) >> (nbits + i);
       nbits++)
    ;

  int b = bits.peekBitsNoFill(15);
  int sign = (b >> 14) * -1;
  int low = (b >> 12) & 3;
  int high = bittable[b & 4095];

  // Skip bytes used above or read bits
  if (high == 12) {
    bits.skipBitsNoFill(15);
    high = bits.getBitsNoFill(16 - nbits) >> 1;
  } else
    bits.skipBitsNoFill(high + 1 + 3);

  (*carry)[0] = (high << nbits) | bits.getBitsNoFill(nbits);
  int diff = ((*carry)[0] ^ sign) + (*carry)[1];
  (*carry)[1] = (diff * 3 + (*carry)[1]) >> 5;
  (*carry)[2] = (*carry)[0] > 16 ? 0 : (*carry)[2] + 1;

  return (diff * 4) | low;
}

inline int OlympusDecompressor::getPred(const Array2DRef<uint16_t> out, int row,
                                        int col) {
  auto getLeft = [&]() { return out(row, col - 2); };
  auto getUp = [&]() { return out(row - 2, col); };
  auto getLeftUp = [&]() { return out(row - 2, col - 2); };

  int pred;
  if (row < 2 && col < 2)
    pred = 0;
  else if (row < 2)
    pred = getLeft();
  else if (col < 2)
    pred = getUp();
  else {
    int left = getLeft();
    int up = getUp();
    int leftUp = getLeftUp();

    int leftMinusNw = left - leftUp;
    int upMinusNw = up - leftUp;

    // Check if sign is different, and they are both not zero
    if ((SignBit(leftMinusNw) ^ SignBit(upMinusNw)) &&
        (leftMinusNw != 0 && upMinusNw != 0)) {
      if (std::abs(leftMinusNw) > 32 || std::abs(upMinusNw) > 32)
        pred = left + upMinusNw;
      else
        pred = (left + up) >> 1;
    } else
      pred = std::abs(leftMinusNw) > std::abs(upMinusNw) ? left : up;
  }

  return pred;
}

void OlympusDecompressor::decompressRow(BitPumpMSB& bits, int row) const {
  assert(mRaw->dim.y > 0);
  assert(mRaw->dim.x > 0);
  assert(mRaw->dim.x % 2 == 0);

  auto rawU16 = dynamic_cast<RawImageDataU16*>(mRaw);
  assert(rawU16);
  const Array2DRef<uint16_t> out(rawU16->getU16DataAsUncroppedArray2DRef());

  std::array<std::array<int, 3>, 2> acarry{{}};

  for (int col = 0; col < out.width; col++) {
    int c = col & 1;

    std::array<int, 3>& carry = acarry[c];

    int diff = parseCarry(bits, &carry);
    int pred = getPred(out, row, col);

    out(row, col) = pred + diff;
  }
}

void OlympusDecompressor::decompress(ByteStream input) const {
  assert(mRaw->dim.y > 0);
  assert(mRaw->dim.x > 0);
  assert(mRaw->dim.x % 2 == 0);

  input.skipBytes(7);
  BitPumpMSB bits(input);

  for (int y = 0; y < mRaw->dim.y; y++)
    decompressRow(bits, y);
}

} // namespace rawspeed
