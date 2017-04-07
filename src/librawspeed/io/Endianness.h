/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2017 Axel Waggershauser

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

#include "common/Common.h" // for uint32, ushort16, uint64, int32, short16
#include <cassert>         // for assert
#include <cstring>         // for memcpy
// IWYU pragma: no_include "io/EndiannessTest.h"

namespace rawspeed {

enum Endianness { big, little, unknown };

inline Endianness getHostEndiannessRuntime() {
  ushort16 testvar = 0xfeff;
  uint32 firstbyte = (reinterpret_cast<uchar8*>(&testvar))[0];
  if (firstbyte == 0xff)
    return little;
  if (firstbyte == 0xfe)
    return big;

  assert(false);

  // Return something to make compilers happy
  return unknown;
}

inline Endianness getHostEndianness() {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  return little;
#elif defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  return big;
#elif defined(__BYTE_ORDER__)
#error "uhm, __BYTE_ORDER__ has some strange value"
#else
  return getHostEndiannessRuntime();
#endif
}

#ifdef _MSC_VER
#include <intrin.h>
#define BSWAP16(A) _byteswap_ushort(A)
#define BSWAP32(A) _byteswap_ulong(A)
#define BSWAP64(A) _byteswap_uint64(A)
#else
#define BSWAP16(A) __builtin_bswap16(A)
#define BSWAP32(A) __builtin_bswap32(A)
#define BSWAP64(A) __builtin_bswap64(A)
#endif

inline short16 getByteSwapped(short16 v) { return BSWAP16(v); }
inline ushort16 getByteSwapped(ushort16 v) { return BSWAP16(v); }
inline int32 getByteSwapped(int32 v) { return BSWAP32(v); }
inline uint32 getByteSwapped(uint32 v) { return BSWAP32(v); }
inline uint64 getByteSwapped(uint64 v) { return BSWAP64(v); }

// the float/double versions use two memcpy which guarantee strict aliasing
// and are compiled into the same assembly as the popular union trick.
inline float getByteSwapped(float f) {
  uint32 i;
  memcpy(&i, &f, sizeof(i));
  i = BSWAP32(i);
  memcpy(&f, &i, sizeof(i));
  return f;
}
inline double getByteSwapped(double d) {
  uint64 i;
  memcpy(&i, &d, sizeof(i));
  i = BSWAP64(i);
  memcpy(&d, &i, sizeof(i));
  return d;
}

template <typename T> inline T getByteSwapped(const void* data, bool bswap) {
  T ret;
  // all interesting compilers optimize this memcpy into a single move
  // this is the most effective way to load some bytes without running into
  // alignment or aliasing issues
  memcpy(&ret, data, sizeof(T));
  return bswap ? getByteSwapped(ret) : ret;
}

// The following functions may be used to get a multi-byte sized tyoe from some
// memory location converted to the native byte order of the host.
// 'BE' suffix: source byte order is known to be big endian
// 'LE' suffix: source byte order is known to be little endian
// Note: these functions should be avoided if higher level acess from
// Buffer/DataBuffer classes is available.

template <typename T> inline T getBE(const void* data) {
  return getByteSwapped<T>(data, getHostEndianness() == little);
}

template <typename T> inline T getLE(const void* data) {
  return getByteSwapped<T>(data, getHostEndianness() == big);
}

inline ushort16 getU16BE(const void* data) { return getBE<ushort16>(data); }
inline ushort16 getU16LE(const void* data) { return getLE<ushort16>(data); }
inline uint32 getU32BE(const void* data) { return getBE<uint32>(data); }
inline uint32 getU32LE(const void* data) { return getLE<uint32>(data); }

#undef BSWAP64
#undef BSWAP32
#undef BSWAP16

} // namespace rawspeed
