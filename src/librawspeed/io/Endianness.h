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

#include "common/Common.h" // for uint32_t, uint16_t, uint64_t, int32_t, int16_t
#include <cassert>         // for assert
#include <cstring>         // for memcpy
// IWYU pragma: no_include "io/EndiannessTest.h"

namespace rawspeed {

enum class Endianness { little = 0xDEAD, big = 0xBEEF, unknown = 0x0BAD };

inline Endianness getHostEndiannessRuntime() {
  uint16_t testvar = 0xfeff;
  uint32_t firstbyte = (reinterpret_cast<uint8_t*>(&testvar))[0];
  if (firstbyte == 0xff)
    return Endianness::little;
  if (firstbyte == 0xfe)
    return Endianness::big;

  assert(false);

  // Return something to make compilers happy
  return Endianness::unknown;
}

inline Endianness getHostEndianness() {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  return Endianness::little;
#elif defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  return Endianness::big;
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

inline int16_t getByteSwapped(int16_t v) {
  return static_cast<int16_t>(BSWAP16(static_cast<uint16_t>(v)));
}
inline uint16_t getByteSwapped(uint16_t v) {
  return static_cast<uint16_t>(BSWAP16(v));
}
inline int32_t getByteSwapped(int32_t v) {
  return static_cast<int32_t>(BSWAP32(static_cast<uint32_t>(v)));
}
inline uint32_t getByteSwapped(uint32_t v) {
  return static_cast<uint32_t>(BSWAP32(v));
}
inline uint64_t getByteSwapped(uint64_t v) {
  return BSWAP64(static_cast<uint64_t>(v));
}

// the float/double versions use two memcpy which guarantee strict aliasing
// and are compiled into the same assembly as the popular union trick.
inline float getByteSwapped(float f) {
  uint32_t i;
  memcpy(&i, &f, sizeof(i));
  i = getByteSwapped(i);
  memcpy(&f, &i, sizeof(i));
  return f;
}
inline double getByteSwapped(double d) {
  uint64_t i;
  memcpy(&i, &d, sizeof(i));
  i = getByteSwapped(i);
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
// Note: these functions should be avoided if higher level access from
// Buffer/DataBuffer classes is available.

template <typename T> inline T getBE(const void* data) {
  return getByteSwapped<T>(data, getHostEndianness() == Endianness::little);
}

template <typename T> inline T getLE(const void* data) {
  return getByteSwapped<T>(data, getHostEndianness() == Endianness::big);
}

inline uint16_t getU16BE(const void* data) { return getBE<uint16_t>(data); }
inline uint16_t getU16LE(const void* data) { return getLE<uint16_t>(data); }
inline uint32_t getU32BE(const void* data) { return getBE<uint32_t>(data); }
inline uint32_t getU32LE(const void* data) { return getLE<uint32_t>(data); }

#undef BSWAP64
#undef BSWAP32
#undef BSWAP16

} // namespace rawspeed
