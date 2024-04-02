/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post

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

#include "rawspeedconfig.h"
#include "adt/Casts.h"
#include "adt/Invariant.h"
#include <algorithm>
#include <bit>
#include <climits>
#include <concepts>
#include <cstdint>
#include <type_traits>

namespace rawspeed {

// only works for positive values and zero
template <typename T> constexpr bool RAWSPEED_READNONE isPowerOfTwo(T val) {
  return (val & (~val + 1)) == val;
}

template <class T>
constexpr unsigned RAWSPEED_READNONE bitwidth([[maybe_unused]] T unused = {}) {
  return CHAR_BIT * sizeof(T);
}

template <class T>
  requires std::unsigned_integral<T>
unsigned numSignBits(const T v) {
  using SignedT = std::make_signed_t<T>;
  return static_cast<SignedT>(v) < 0 ? std::countl_one(v) : std::countl_zero(v);
}

template <class T>
  requires(std::unsigned_integral<T> && bitwidth<T>() >= bitwidth<uint32_t>())
unsigned numActiveBits(const T v) {
  return bitwidth(v) - std::countl_zero(v);
}

template <class T>
  requires(std::unsigned_integral<T> && bitwidth<T>() < bitwidth<uint32_t>())
unsigned numActiveBits(const T v) {
  return numActiveBits(static_cast<uint32_t>(v));
}

template <class T>
  requires std::unsigned_integral<T>
unsigned numSignificantBits(const T v) {
  return bitwidth(v) - numSignBits(v) + 1;
}

// Clamps the given value to the range 0 .. 2^n-1, with n <= 16
template <typename T>
  requires std::is_arithmetic_v<T>
constexpr auto RAWSPEED_READNONE clampBits(T value, unsigned int nBits) {
  // We expect to produce uint16_t.
  invariant(nBits <= 16);
  // Check that the clamp is not a no-op. Not of uint16_t to 16 bits e.g.
  // (Well, not really, if we are called from clampBits<signed>, it's ok..).
  invariant(bitwidth<T>() > nBits); // If nBits >= bitwidth, then shift is UB.
  const auto maxVal = implicit_cast<T>((T(1) << nBits) - T(1));
  return implicit_cast<uint16_t>(std::clamp(value, T(0), maxVal));
}

template <typename T>
  requires std::is_arithmetic_v<T>
constexpr bool RAWSPEED_READNONE isIntN(T value, unsigned int nBits) {
  invariant(nBits < bitwidth<T>() && "Check must not be tautological.");
  using UnsignedT = std::make_unsigned_t<T>;
  const auto highBits = static_cast<UnsignedT>(value) >> nBits;
  return highBits == 0;
}

template <class T>
  requires std::unsigned_integral<T>
constexpr RAWSPEED_READNONE T extractLowBits(T value, unsigned nBits) {
  // invariant(nBits >= 0);
  invariant(nBits != 0);             // Would result in out-of-bound shift.
  invariant(nBits <= bitwidth<T>()); // No-op is fine.
  unsigned numHighPaddingBits = bitwidth<T>() - nBits;
  // invariant(numHighPaddingBits >= 0);
  invariant(numHighPaddingBits < bitwidth<T>()); // Shift is in-bounds.
  value <<= numHighPaddingBits;
  value >>= numHighPaddingBits;
  return value;
}

template <class T>
  requires std::unsigned_integral<T>
constexpr RAWSPEED_READNONE T extractLowBitsSafe(T value, unsigned nBits) {
  // invariant(nBits >= 0);
  invariant(nBits <= bitwidth<T>());
  if (nBits == 0)
    return 0;
  return extractLowBits(value, nBits);
}

template <class T>
  requires std::is_unsigned_v<T>
constexpr RAWSPEED_READNONE T extractHighBits(
    T value, unsigned nBits, unsigned effectiveBitwidth = bitwidth<T>()) {
  invariant(effectiveBitwidth <= bitwidth<T>());
  invariant(nBits <= effectiveBitwidth);
  auto numLowBitsToSkip = effectiveBitwidth - nBits;
  invariant(numLowBitsToSkip < bitwidth<T>());
  return value >> numLowBitsToSkip;
}

template <typename T>
  requires std::is_unsigned_v<T>
constexpr typename std::make_signed_t<T>
    RAWSPEED_READNONE signExtend(T value, unsigned int nBits) {
  invariant(nBits != 0 && "Only valid for non-zero bit count.");
  const T SpareSignBits = bitwidth<T>() - nBits;
  using SignedT = std::make_signed_t<T>;
  return static_cast<SignedT>(value << SpareSignBits) >> SpareSignBits;
}

} // namespace rawspeed
