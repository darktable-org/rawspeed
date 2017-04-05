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

#include <algorithm>        // for forward
#include <cassert>          // for assert
#include <cstdint>          // for uintptr_t
#include <cstring>          // for memcpy, size_t
#include <initializer_list> // for initializer_list
#include <memory>           // for unique_ptr, allocator
#include <string>           // for string
#include <vector>           // for vector

int rawspeed_get_number_of_processor_cores();

namespace RawSpeed {

using char8 = signed char;
using uchar8 = unsigned char;
using uint32 = unsigned int;
using int64 = long long;
using uint64 = unsigned long long;
using int32 = signed int;
using ushort16 = unsigned short;
using short16 = signed short;

enum DEBUG_PRIO {
  DEBUG_PRIO_ERROR = 0x10,
  DEBUG_PRIO_WARNING = 0x100,
  DEBUG_PRIO_INFO = 0x1000,
  DEBUG_PRIO_EXTRA = 0x10000
};

void writeLog(DEBUG_PRIO priority, const char* format, ...)
    __attribute__((format(printf, 2, 3)));

inline void copyPixels(uchar8* dest, int dstPitch, const uchar8* src,
                       int srcPitch, int rowSize, int height)
{
  if (height == 1 || (dstPitch == srcPitch && srcPitch == rowSize))
    memcpy(dest, src, (size_t)rowSize * height);
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
roundUp(size_t value, size_t multiple) {
  return ((multiple == 0) || (value % multiple == 0))
             ? value
             : value + multiple - (value % multiple);
}

template <typename T>
constexpr inline __attribute__((const)) bool isAligned(T value,
                                                       size_t multiple) {
  return (multiple == 0) || ((uintptr_t)value % multiple == 0);
}

template <typename T, typename T2>
bool __attribute__((pure))
isIn(const T value, const std::initializer_list<T2>& list) {
  for (auto t : list)
    if (t == value)
      return true;
  return false;
}

// until we allow c++14 code
#if __cplusplus < 201402L
template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...)); // NOLINT
}
#endif

inline uint32 getThreadCount()
{
#ifndef HAVE_PTHREAD
  return 1;
#elif defined(WIN32)
  return pthread_num_processors_np();
#else
  return rawspeed_get_number_of_processor_cores();
#endif
}

// clampBits clamps the given int to the range 0 .. 2^n-1, with n <= 16
inline ushort16 __attribute__((const)) clampBits(int x, uint32 n) {
  assert(n <= 16);
  const int tmp = (1 << n) - 1;
  x = x < 0 ? 0 : x;
  x = x > tmp ? tmp : x;
  return x;
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

    while (*str != c && *str)
      str++;

    if (begin != str)
      result.emplace_back(begin, str);

    if (0 == *str++)
      break;
  }

  return result;
}

enum BitOrder {
  BitOrder_Plain,  /* Memory order */
  BitOrder_Jpeg,   /* Input is added to stack byte by byte, and output is lifted from top */
  BitOrder_Jpeg16, /* Same as above, but 16 bits at the time */
  BitOrder_Jpeg32, /* Same as above, but 32 bits at the time */
};

// little 'forced' loop unrolling helper tool, example:
//   unroll_loop<N>([&](int i) {
//     func(i);
//   });
// will translate to:
//   func(0); func(1); func(2); ... func(N-1);

template <typename Lambda, size_t N>
struct unroll_loop_t {
  inline static void repeat(const Lambda& f) {
    unroll_loop_t<Lambda, N-1>::repeat(f);
    f(N-1);
  }
};

template <typename Lambda>
struct unroll_loop_t<Lambda, 0> {
  inline static void repeat(const Lambda& f) {
    // this method is correctly empty.
    // only needed as part of compile time 'manual' branch unrolling
  }
};

template <size_t N, typename Lambda>
inline void unroll_loop(const Lambda& f) {
  unroll_loop_t<Lambda, N>::repeat(f);
}

} // namespace RawSpeed
