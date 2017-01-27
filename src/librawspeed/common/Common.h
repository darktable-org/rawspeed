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

#include <algorithm>        // for forward
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
using uint64 = unsigned long long;
using int32 = signed int;
using ushort16 = unsigned short;
using short16 = signed short;

enum Endianness { big, little, unknown };

const int DEBUG_PRIO_ERROR = 0x10;
const int DEBUG_PRIO_WARNING = 0x100;
const int DEBUG_PRIO_INFO = 0x1000;
const int DEBUG_PRIO_EXTRA = 0x10000;

void writeLog(int priority, const char *format, ...) __attribute__((format(printf, 2, 3)));

inline void BitBlt(uchar8* dstp, int dst_pitch, const uchar8* srcp, int src_pitch, int row_size, int height) {
  if (height == 1 || (dst_pitch == src_pitch && src_pitch == row_size)) {
    memcpy(dstp, srcp, (size_t)row_size * height);
    return;
  }
  for (int y=height; y>0; --y) {
    memcpy(dstp, srcp, row_size);
    dstp += dst_pitch;
    srcp += src_pitch;
  }
}

template <typename T> inline constexpr bool isPowerOfTwo(T val) {
  return (val & (~val+1)) == val;
}

constexpr inline size_t roundUp(size_t value, size_t multiple) {
  return ((multiple == 0) || (value % multiple == 0))
             ? value
             : value + multiple - (value % multiple);
}

template<typename T> bool isIn(const T value, const std::initializer_list<T>& list) {
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
#ifdef NO_PTHREAD
  return 1;
#elif defined(WIN32)
  return pthread_num_processors_np();
#else
  return rawspeed_get_number_of_processor_cores();
#endif
}

inline Endianness getHostEndianness() {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  return little;
#elif defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  return big;
#else
  ushort16 testvar = 0xfeff;
  uint32 firstbyte = ((uchar8 *)&testvar)[0];
  if (firstbyte == 0xff)
    return little;
  else if (firstbyte == 0xfe)
    return big;
  else
    assert(false);

  // Return something to make compilers happy
  return unknown;
#endif
}

#ifdef _MSC_VER
# include <intrin.h>
# define BSWAP16(A) _byteswap_ushort(A)
# define BSWAP32(A) _byteswap_ulong(A)
# define BSWAP64(A) _byteswap_uint64(A)
#else
# define BSWAP16(A) __builtin_bswap16(A)
# define BSWAP32(A) __builtin_bswap32(A)
# define BSWAP64(A) __builtin_bswap64(A)
#endif

template<typename T> inline T getByteSwapped(const void* data, bool bswap)
{
  T ret;
  // all interesting compilers optimize this memcpy into a single move
  // this is the most effective way to load some bytes without running into alignmen issues
  memcpy(&ret, data, sizeof(T));
  if (bswap) {
    switch(sizeof(T)) {
    case 1: break;
    case 2: ret = BSWAP16(ret); break;
    case 4: ret = BSWAP32(ret); break;
    case 8: ret = BSWAP64(ret); break;
    }
  }
  return ret;
}

// The following functions may be used to get a multi-byte sized tyoe from some
// memory location converted to the native byte order of the host.
// 'BE' suffix: source byte order is known to be big endian
// 'LE' suffix: source byte order is known to be little endian
// Note: these functions should be avoided if higher level acess from
// Buffer/DataBuffer classes is available.

template <typename T> inline T getBE(const void* data)
{
  return getByteSwapped<T>(data, getHostEndianness() == little);
}

template <typename T> inline T getLE(const void* data)
{
  return getByteSwapped<T>(data, getHostEndianness() == big);
}

inline ushort16 getU16BE(const void* data) { return getBE<ushort16>(data); }
inline ushort16 getU16LE(const void* data) { return getLE<ushort16>(data); }
inline uint32 getU32BE(const void* data)   { return getBE<uint32>(data); }
inline uint32 getU32LE(const void* data)   { return getLE<uint32>(data); }

#ifdef _MSC_VER
// See http://tinyurl.com/hqfuznc
#if _MSC_VER >= 1900
extern "C" { FILE __iob_func[3] = { *stdin,*stdout,*stderr }; }
#endif
#endif

inline uint32 clampbits(int x, uint32 n) {
  uint32 _y_temp;
  if( (_y_temp=x>>n) )
    x = ~_y_temp >> (32-n);
  return x;
}

/* Remove all spaces at the end of a string */

inline void TrimSpaces(std::string& str) {
  // Trim Both leading and trailing spaces
  size_t startpos = str.find_first_not_of(" \t"); // Find the first character position after excluding leading blank spaces
  size_t endpos = str.find_last_not_of(" \t"); // Find the first character position from reverse af

  // if all spaces or empty return an empty string
  if ((std::string::npos == startpos) || (std::string::npos == endpos)) {
    str = "";
  } else
    str = str.substr(startpos, endpos - startpos + 1);
}

inline std::vector<std::string> split_string(const std::string &input,
                                             char c = ' ') {
  std::vector<std::string> result;
  const char *str = input.c_str();

  while (true) {
    const char *begin = str;

    while(*str != c && *str)
      str++;

    if(begin != str)
      result.emplace_back(begin, str);

    if(0 == *str++)
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
  inline static void repeat(const Lambda& f) {}
};

template <size_t N, typename Lambda>
inline void unroll_loop(const Lambda& f) {
  unroll_loop_t<Lambda, N>::repeat(f);
}

} // namespace RawSpeed
