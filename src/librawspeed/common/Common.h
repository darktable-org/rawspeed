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

#include <algorithm>        // IWYU pragma: keep
#include <cassert>          // for assert
#include <climits>          // for CHAR_BIT
#include <cstdint>          // for uint8_t, uint16_t, uintptr_t
#include <cstring>          // for size_t, memcpy
#include <initializer_list> // for initializer_list
#include <string>           // for string
#include <type_traits>      // for enable_if, is_unsigned, is_pointer, make...
#include <vector>           // for vector

extern "C" int rawspeed_get_number_of_processor_cores();

namespace rawspeed {

enum class DEBUG_PRIO {
  ERROR = 0x10,
  WARNING = 0x100,
  INFO = 0x1000,
  EXTRA = 0x10000
};

void writeLog(DEBUG_PRIO priority, const char* format, ...)
    __attribute__((format(printf, 2, 3)));

inline void copyPixels(uint8_t* dest, int dstPitch, const uint8_t* src,
                       int srcPitch, int rowSize, int height) {
  if (height == 1 || (dstPitch == srcPitch && srcPitch == rowSize))
    memcpy(dest, src, static_cast<size_t>(rowSize) * height);
  else {
    for (int y = height; y > 0; --y) {
      memcpy(dest, src, rowSize);
      dest += dstPitch;
      src += srcPitch;
    }
  }
}

// only works for positive values and zero
template <typename T> inline constexpr bool isPowerOfTwo(T val) {
  return (val & (~val+1)) == val;
}

constexpr inline size_t __attribute__((const))
roundToMultiple(size_t value, size_t multiple, bool roundDown) {
  if ((multiple == 0) || (value % multiple == 0))
    return value;
  // Drop remainder.
  size_t roundedDown = value - (value % multiple);
  if (roundDown) // If we were rounding down, then that's it.
    return roundedDown;
  // Else, just add one multiple.
  return roundedDown + multiple;
}

constexpr inline size_t __attribute__((const))
roundDown(size_t value, size_t multiple) {
  return roundToMultiple(value, multiple, /*roundDown=*/true);
}

constexpr inline size_t __attribute__((const))
roundUp(size_t value, size_t multiple) {
  return roundToMultiple(value, multiple, /*roundDown=*/false);
}

constexpr inline size_t __attribute__((const))
roundUpDivision(size_t value, size_t div) {
  return (value != 0) ? (1 + ((value - 1) / div)) : 0;
}

template <class T>
inline constexpr __attribute__((const)) bool isAligned(
    T value, size_t multiple,
    typename std::enable_if_t<std::is_pointer_v<T>>* /*unused*/ = nullptr) {
  return (multiple == 0) ||
         (reinterpret_cast<std::uintptr_t>(value) % multiple == 0);
}

template <class T>
inline constexpr __attribute__((const)) bool isAligned(
    T value, size_t multiple,
    typename std::enable_if_t<!std::is_pointer_v<T>>* /*unused*/ = nullptr) {
  return (multiple == 0) ||
         (static_cast<std::uintptr_t>(value) % multiple == 0);
}

template <typename T, typename T2>
bool __attribute__((pure))
isIn(const T value, const std::initializer_list<T2>& list) {
  return std::any_of(list.begin(), list.end(),
                     [value](const T2& t) { return t == value; });
}

template <class T> inline constexpr unsigned bitwidth(T unused = {}) {
  return CHAR_BIT * sizeof(T);
}

// Clamps the given unsigned value to the range 0 .. 2^n-1, with n <= 16
template <class T>
inline constexpr __attribute__((const)) uint16_t clampBits(
    T value, unsigned int nBits,
    typename std::enable_if_t<std::is_unsigned_v<T>>* /*unused*/ = nullptr) {
  // We expect to produce uint16_t.
  assert(nBits <= 16);
  // Check that the clamp is not a no-op. Not of uint16_t to 16 bits e.g.
  // (Well, not really, if we are called from clampBits<signed>, it's ok..).
  assert(bitwidth<T>() > nBits); // If nBits >= bitwidth, then shift is UB.
  const T maxVal = (T(1) << nBits) - T(1);
  return std::min(value, maxVal);
}

// Clamps the given signed value to the range 0 .. 2^n-1, with n <= 16
template <typename T>
inline constexpr uint16_t __attribute__((const)) clampBits(
    T value, unsigned int nBits,
    typename std::enable_if_t<std::is_signed_v<T>>* /*unused*/ = nullptr) {
  // If the value is negative, clamp it to zero.
  value = std::max(value, T(0));
  // Now, let the unsigned case clamp to the upper limit.
  using UnsignedT = std::make_unsigned_t<T>;
  return clampBits<UnsignedT>(value, nBits);
}

template <typename T>
inline constexpr bool __attribute__((const)) isIntN(
    T value, unsigned int nBits,
    typename std::enable_if_t<std::is_arithmetic_v<T>>* /*unused*/ = nullptr) {
  assert(nBits < bitwidth<T>() && "Check must not be tautological.");
  using UnsignedT = std::make_unsigned_t<T>;
  const auto highBits = static_cast<UnsignedT>(value) >> nBits;
  return highBits == 0;
}

template <class T>
inline constexpr __attribute__((const)) T extractHighBits(
    T value, unsigned nBits, unsigned effectiveBitwidth = bitwidth<T>(),
    typename std::enable_if_t<std::is_unsigned_v<T>>* /*unused*/ = nullptr) {
  assert(effectiveBitwidth <= bitwidth<T>());
  assert(nBits <= effectiveBitwidth);
  auto numLowBitsToSkip = effectiveBitwidth - nBits;
  assert(numLowBitsToSkip < bitwidth<T>());
  return value >> numLowBitsToSkip;
}

template <typename T>
inline constexpr typename std::make_signed_t<T> __attribute__((const))
signExtend(
    T value, unsigned int nBits,
    typename std::enable_if_t<std::is_unsigned_v<T>>* /*unused*/ = nullptr) {
  assert(nBits != 0 && "Only valid for non-zero bit count.");
  const T SpareSignBits = bitwidth<T>() - nBits;
  using SignedT = std::make_signed_t<T>;
  return static_cast<SignedT>(value << SpareSignBits) >> SpareSignBits;
}

// Trim both leading and trailing spaces from the string
inline std::string trimSpaces(const std::string& str)
{
  // Find the first character position after excluding leading blank spaces
  size_t startpos = str.find_first_not_of(" \t");

  // Find the first character position from reverse af
  size_t endpos = str.find_last_not_of(" \t");

  // if all spaces or empty return an empty string
  if ((startpos == std::string::npos) || (endpos == std::string::npos))
    return "";

  return str.substr(startpos, endpos - startpos + 1);
}

inline std::vector<std::string> splitString(const std::string& input,
                                            char c = ' ')
{
  std::vector<std::string> result;
  const char* str = input.c_str();

  while (true) {
    const char* begin = str;

    while (*str != c && *str != '\0')
      str++;

    if (begin != str)
      result.emplace_back(begin, str);

    const bool isNullTerminator = (*str == '\0');
    str++;

    if (isNullTerminator)
      break;
  }

  return result;
}

enum class BitOrder {
  LSB,   /* Memory order */
  MSB,   /* Input is added to stack byte by byte, and output is lifted
                     from top */
  MSB16, /* Same as above, but 16 bits at the time */
  MSB32, /* Same as above, but 32 bits at the time */
};

} // namespace rawspeed
