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
#include "adt/Array1DRef.h"
#include "adt/Array2DRef.h"
#include "adt/Bit.h"
#include "adt/Invariant.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

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

inline void copyPixelsImpl(Array1DRef<std::byte> dest,
                           Array1DRef<const std::byte> src) {
  invariant(src.size() == dest.size());
  std::copy(src.begin(), src.end(), dest.begin());
}

inline void copyPixelsImpl(Array2DRef<std::byte> dest,
                           Array2DRef<const std::byte> src) {
  invariant(src.width() > 0);
  invariant(src.height() > 0);
  invariant(dest.width() > 0);
  invariant(dest.height() > 0);
  invariant(src.height() == dest.height());
  invariant(src.width() == dest.width());
  if (auto [destAsStrip, srcAsStrip] =
          std::make_tuple(dest.getAsArray1DRef(), src.getAsArray1DRef());
      destAsStrip && srcAsStrip) {
    copyPixelsImpl(*destAsStrip, *srcAsStrip);
    return;
  }
  for (int row = 0; row != src.height(); ++row)
    copyPixelsImpl(dest[row], src[row]);
}

inline void copyPixels(std::byte* destPtr, int dstPitch,
                       const std::byte* srcPtr, int srcPitch, int rowSize,
                       int height) {
  invariant(destPtr);
  invariant(dstPitch > 0);
  invariant(srcPtr);
  invariant(srcPitch > 0);
  invariant(rowSize > 0);
  invariant(height > 0);
  invariant(rowSize <= srcPitch);
  invariant(rowSize <= dstPitch);
  auto dest = Array2DRef(destPtr, rowSize, height, dstPitch);
  auto src = Array2DRef(srcPtr, rowSize, height, srcPitch);
  copyPixelsImpl(dest, src);
}

template <typename T>
  requires std::is_pointer_v<T>
constexpr uint64_t RAWSPEED_READNONE getMisalignmentOffset(T value,
                                                           uint64_t multiple) {
  if (multiple == 0)
    return 0;
  static_assert(bitwidth<uintptr_t>() >= bitwidth<T>(),
                "uintptr_t can not represent all pointer values?");
  return reinterpret_cast<uintptr_t>(value) % multiple;
}

template <typename T>
  requires std::is_integral_v<T>
constexpr uint64_t RAWSPEED_READNONE getMisalignmentOffset(T value,
                                                           uint64_t multiple) {
  if (multiple == 0)
    return 0;
  return value % multiple;
}

template <typename T>
constexpr T RAWSPEED_READNONE roundToMultiple(T value, uint64_t multiple,
                                              bool roundDown) {
  uint64_t offset = getMisalignmentOffset(value, multiple);
  if (offset == 0)
    return value;
  // Drop remainder.
  T roundedDown = value - offset;
  if (roundDown) // If we were rounding down, then that's it.
    return roundedDown;
  // Else, just add one multiple.
  return roundedDown + multiple;
}

constexpr uint64_t RAWSPEED_READNONE roundDown(uint64_t value,
                                               uint64_t multiple) {
  return roundToMultiple(value, multiple, /*roundDown=*/true);
}

constexpr uint64_t RAWSPEED_READNONE roundUp(uint64_t value,
                                             uint64_t multiple) {
  return roundToMultiple(value, multiple, /*roundDown=*/false);
}

constexpr uint64_t RAWSPEED_READNONE roundUpDivision(uint64_t value,
                                                     uint64_t div) {
  invariant(div != 0);
  return roundUp(value, div) / div;
}

constexpr uint64_t RAWSPEED_READNONE roundUpDivisionSafe(uint64_t value,
                                                         uint64_t div) {
  return (value != 0) ? (1 + ((value - 1) / div)) : 0;
}

template <class T>
constexpr RAWSPEED_READNONE bool isAligned(T value, size_t multiple) {
  return (multiple == 0) || (getMisalignmentOffset(value, multiple) == 0);
}

template <typename T, typename T2>
bool RAWSPEED_READONLY isIn(const T value,
                            const std::initializer_list<T2>& list) {
  return std::any_of(list.begin(), list.end(),
                     [value](const T2& t) { return t == value; });
}

// Trim both leading and trailing spaces from the string
inline std::string trimSpaces(std::string_view str) {
  // Find the first character position after excluding leading blank spaces
  size_t startpos = str.find_first_not_of(" \t");

  // Find the first character position from reverse af
  size_t endpos = str.find_last_not_of(" \t");

  // if all spaces or empty return an empty string
  if ((startpos == std::string::npos) || (endpos == std::string::npos))
    return "";

  str = str.substr(startpos, endpos - startpos + 1);
  return {str.begin(), str.end()};
}

inline std::vector<std::string> splitString(const std::string& input,
                                            char c = ' ') {
  std::vector<std::string> result;

  std::string_view str = input;
  while (!str.empty()) {
    std::string_view::size_type pos = str.find_first_of(c);

    if (pos == std::string_view::npos)
      pos = str.size();

    auto substr = str.substr(/*pos=*/0, /*n=*/pos);

    if (!substr.empty())
      result.emplace_back(substr);

    str.remove_prefix(std::min(str.size(), 1 + substr.size()));
  }

  return result;
}

template <int N, typename T>
inline std::array<T, N> to_array(const std::vector<T>& v) {
  std::array<T, N> a;
  assert(v.size() == N && "Size mismatch");
  std::move(v.begin(), v.end(), a.begin());
  return a;
}

} // namespace rawspeed
