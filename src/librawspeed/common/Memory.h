/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2017 Roman Lebedev

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

#include "common/Common.h" // for isPowerOfTwo
#include <cstddef>         // for size_t
#include <cstdint>         // for SIZE_MAX

namespace rawspeed {

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-attributes"
#pragma GCC diagnostic ignored "-Wattributes"

// coverity[+alloc]
void* alignedMalloc(size_t size, size_t alignment)
    __attribute__((malloc, warn_unused_result, alloc_size(1), alloc_align(2),
                   deprecated("use alignedMalloc<alignment>(size)")));

template <typename T, size_t alignment>
// coverity[+alloc]
inline T* __attribute__((malloc, warn_unused_result, alloc_size(1)))
alignedMalloc(size_t size) {
  static_assert(alignment >= alignof(T), "unsufficient alignment");
  static_assert(isPowerOfTwo(alignment), "not power-of-two");
  static_assert(isAligned(alignment, sizeof(void*)),
                "not multiple of sizeof(void*)");

#if !(defined(HAVE_POSIX_MEMALIGN) || defined(HAVE_ALIGNED_ALLOC) ||           \
      defined(HAVE_MM_MALLOC) || defined(HAVE_ALIGNED_MALLOC))
  static_assert(alignment <= alignof(std::max_align_t), "too high alignment");
#if defined(__APPLE__)
  // apple malloc() aligns to 16 by default. can not expect any more
  static_assert(alignment <= 16, "on OSX, plain malloc() aligns to 16");
#endif
#endif

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  return reinterpret_cast<T*>(alignedMalloc(size, alignment));
}

#pragma GCC diagnostic pop

template <typename T, size_t alignment, bool doRoundUp = false>
// coverity[+alloc]
inline T* __attribute__((malloc, warn_unused_result))
alignedMallocArray(size_t nmemb, size_t size) {
  // Check for size_t overflow
  if (size && nmemb > SIZE_MAX / size)
    return nullptr;

  size *= nmemb;

  if (doRoundUp)
    size = roundUp(size, alignment);

  return alignedMalloc<T, alignment>(size);
}

template <typename T, size_t alignment, typename T2, bool doRoundUp = false>
// coverity[+alloc]
inline T* __attribute__((malloc, warn_unused_result))
alignedMallocArray(size_t nmemb) {
  static_assert(sizeof(T), "???");
  static_assert(sizeof(T2), "???");
  static_assert(alignment >= alignof(T), "unsufficient alignment");
  static_assert(alignment >= alignof(T2), "unsufficient alignment");
  static_assert(isPowerOfTwo(sizeof(T2)), "not power-of-two");

  return alignedMallocArray<T, alignment, doRoundUp>(nmemb, sizeof(T2));
}

// coverity[+free : arg-0]
void alignedFree(void* ptr);

// coverity[+free : arg-0]
void alignedFreeConstPtr(const void* ptr);

} // namespace rawspeed
