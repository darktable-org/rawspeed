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
#include <cstdint>          // for UINT32_MAX
#include <cstring>          // for memcpy, size_t
#include <initializer_list> // for initializer_list
#include <memory>           // for unique_ptr, allocator
#include <string>           // for string
#include <vector>           // for vector

#if !defined(__unix__) && !defined(__APPLE__) && !defined(__MINGW32__)
#include <intrin.h>
#pragma intrinsic(_ReturnAddress)
#define MIN(a,b) min(a,b)
#define MAX(a,b) max(a,b)
typedef unsigned __int64 uint64;
// MSVC may not have NAN
#ifndef NAN
  static const unsigned long __nan[2] = {0xffffffff, 0x7fffffff};
  #define NAN (*(const float *) __nan)
#endif
#else // On linux
#define _ASSERTE(a) void(a)
#define _RPT0(a,b)
#define _RPT1(a,b,c)
#define _RPT2(a,b,c,d)
#define _RPT3(a,b,c,d,e)
#define _RPT4(a,b,c,d,e,f)
#define __inline inline
#define _strdup(a) strdup(a)
#ifndef MIN
#define MIN(a, b)  lmin(a,b)
#endif
#ifndef MAX
#define MAX(a, b)  lmax(a,b)
#endif
typedef unsigned long long uint64;
#ifndef __MINGW32__
void* _aligned_malloc(size_t bytes, size_t alignment);
void _aligned_free(void *ptr);
#endif
#endif // __unix__

#ifndef UINT32_MAX
#define UINT32_MAX 0xffffffff
#endif

int rawspeed_get_number_of_processor_cores();


namespace RawSpeed {

typedef signed char char8;
typedef unsigned char uchar8;
typedef unsigned int uint32;
typedef signed int int32;
typedef unsigned short ushort16;
typedef signed short short16;

typedef enum Endianness {
  big, little, unknown
} Endianness;

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
inline bool isPowerOfTwo (int val) {
  return (val & (~val+1)) == val;
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

inline int lmin(int p0, int p1) {
  return p1 + ((p0 - p1) & ((p0 - p1) >> 31));
}
inline int lmax(int p0, int p1) {
  return p0 - ((p0 - p1) & ((p0 - p1) >> 31));
}

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
    _ASSERTE(false);

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

template<typename T> inline T loadMem(const void* data, bool bswap) {
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

#define get2BE(data, pos)                                                      \
  (loadMem<ushort16>((data) + (pos), getHostEndianness() == little))
#define get2LE(data, pos)                                                      \
  (loadMem<ushort16>((data) + (pos), getHostEndianness() == big))

#define get4BE(data, pos)                                                      \
  (loadMem<uint32>((data) + (pos), getHostEndianness() == little))
#define get4LE(data, pos)                                                      \
  (loadMem<uint32>((data) + (pos), getHostEndianness() == big))

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

/* This is faster - at least when compiled on visual studio 32 bits */
inline int other_abs(int x) { int const mask = x >> 31; return (x + mask) ^ mask;}

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

typedef enum {
  BitOrder_Plain,  /* Memory order */
  BitOrder_Jpeg,   /* Input is added to stack byte by byte, and output is lifted from top */
  BitOrder_Jpeg16, /* Same as above, but 16 bits at the time */
  BitOrder_Jpeg32, /* Same as above, but 32 bits at the time */
} BitOrder;

} // namespace RawSpeed
