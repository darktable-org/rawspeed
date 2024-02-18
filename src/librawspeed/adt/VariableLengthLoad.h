/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2024 Roman Lebedev

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

#include "adt/Array1DRef.h"
#include "adt/Bit.h"
#include "adt/Casts.h"
#include "adt/CroppedArray1DRef.h"
#include "adt/Invariant.h"
#include "io/Endianness.h"
#include <algorithm>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace rawspeed {

namespace impl {

template <typename T> struct zext {};

template <> struct zext<uint8_t> {
  using type = uint16_t;
};
template <> struct zext<uint16_t> {
  using type = uint32_t;
};
template <> struct zext<uint32_t> {
  using type = uint64_t;
};

template <typename T>
concept CanZExt = requires { typename zext<T>::type; };

template <typename T>
  requires std::is_unsigned_v<T> && CanZExt<T>
inline T logicalRightShiftSafe(T val, int shAmt) {
  invariant(shAmt >= 0);
  shAmt = std::min(shAmt, implicit_cast<int>(bitwidth<T>()));
  using WideT = typename zext<T>::type;
  auto valWide = static_cast<WideT>(val);
  valWide >>= shAmt;
  return implicit_cast<T>(valWide);
}

template <typename T>
  requires std::is_unsigned_v<T> && (!CanZExt<T>)
inline T logicalRightShiftSafe(T val, int shAmt) {
  invariant(shAmt >= 0);
  if (shAmt >= implicit_cast<int>(bitwidth<T>()))
    return 0;
  val >>= shAmt;
  return val;
}

template <typename T>
  requires std::is_unsigned_v<T>
inline void variableLengthLoad(Array1DRef<std::byte> out,
                               Array1DRef<const std::byte> in, int inPos) {
  invariant(out.size() == sizeof(T));

  int inPosFixup = 0;
  if (int inPosEnd = inPos + out.size(); in.size() < inPosEnd)
    inPosFixup = in.size() - inPosEnd;
  invariant(inPosFixup <= 0);

  inPos += inPosFixup;

  in = in.getCrop(inPos, out.size()).getAsArray1DRef();
  invariant(in.size() == out.size());

  auto tmp = getLE<T>(in.begin());

  int posMismatchBits = CHAR_BIT * (-inPosFixup);
  tmp = logicalRightShiftSafe(tmp, posMismatchBits);

  tmp = getLE<T>(&tmp);
  memcpy(out.begin(), &tmp, sizeof(T));
}

} // namespace impl

inline void variableLengthLoad(const Array1DRef<std::byte> out,
                               Array1DRef<const std::byte> in, int inPos) {

  invariant(out.size() != 0);
  invariant(isPowerOfTwo(out.size()));
  invariant(out.size() <= 8);
  invariant(in.size() != 0);
  invariant(out.size() <= in.size());
  invariant(inPos >= 0);

  switch (out.size()) {
  case 1:
    impl::variableLengthLoad<uint8_t>(out, in, inPos);
    return;
  case 2:
    impl::variableLengthLoad<uint16_t>(out, in, inPos);
    return;
  case 4:
    impl::variableLengthLoad<uint32_t>(out, in, inPos);
    return;
  case 8:
    impl::variableLengthLoad<uint64_t>(out, in, inPos);
    return;
  default:
    __builtin_unreachable();
  }
}

inline void variableLengthLoadNaiveViaConditionalLoad(
    Array1DRef<std::byte> out, Array1DRef<const std::byte> in, int inPos) {
  invariant(out.size() != 0);
  invariant(in.size() != 0);
  invariant(out.size() <= in.size());
  invariant(inPos >= 0);

  std::fill(out.begin(), out.end(), std::byte{0x00});

  for (int outIndex = 0; outIndex != out.size(); ++outIndex) {
    const int inIndex = inPos + outIndex;
    if (inIndex >= in.size())
      return;
    out(outIndex) = in(inIndex); // masked load
  }
}

inline void variableLengthLoadNaiveViaMemcpy(Array1DRef<std::byte> out,
                                             Array1DRef<const std::byte> in,
                                             int inPos) {
  invariant(out.size() != 0);
  invariant(in.size() != 0);
  invariant(out.size() <= in.size());
  invariant(inPos >= 0);

  std::fill(out.begin(), out.end(), std::byte{0x00});

  inPos = std::min(inPos, in.size());

  int inPosEnd = inPos + out.size();
  inPosEnd = std::min(inPosEnd, in.size());
  invariant(inPos <= inPosEnd);

  const int copySize = inPosEnd - inPos;
  invariant(copySize >= 0);
  invariant(copySize <= out.size());

  out = out.getCrop(/*offset=*/0, copySize).getAsArray1DRef();
  in = in.getCrop(/*offset=*/inPos, copySize).getAsArray1DRef();
  invariant(in.size() == out.size());

  memcpy(out.begin(), in.begin(), copySize);
}

} // namespace rawspeed
