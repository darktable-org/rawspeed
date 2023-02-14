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
#include "common/Common.h" // for isPowerOfTwo, isAligned, roundUp
#include <cstddef>         // for size_t
#include <cstdint>         // for SIZE_MAX

namespace rawspeed {

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-attributes"
#pragma GCC diagnostic ignored "-Wattributes"

// coverity[+alloc]
inline void* __attribute__((malloc, warn_unused_result, alloc_size(1),
                            alloc_align(2),
                            deprecated("use alignedMalloc<alignment>(size)")))
alignedMalloc(size_t size, size_t alignment) {
  assert(isPowerOfTwo(alignment)); // for posix_memalign, _aligned_malloc
  assert(isAligned(alignment, sizeof(void*))); // for posix_memalign
  assert(isAligned(size, alignment));          // for aligned_alloc

  void* ptr = nullptr;

#if defined(HAVE_ALIGNED_ALLOC)
  ptr = aligned_alloc(alignment, size);
#elif defined(HAVE_POSIX_MEMALIGN)
  if (0 != posix_memalign(&ptr, alignment, size))
    return nullptr;
#elif defined(HAVE_ALIGNED_MALLOC)
  ptr = _aligned_malloc(size, alignment);
#else
#error "No aligned malloc() implementation available!"
#endif

  assert(isAligned(ptr, alignment));

  return ptr;
}

template <typename T, size_t alignment>
// coverity[+alloc]
inline T* __attribute__((malloc, warn_unused_result, alloc_size(1)))
alignedMalloc(size_t size) {
  static_assert(alignment >= alignof(T), "insufficient alignment");
  static_assert(isPowerOfTwo(alignment), "not power-of-two");
  static_assert(isAligned(alignment, sizeof(void*)),
                "not multiple of sizeof(void*)");

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
  static_assert(alignment >= alignof(T), "insufficient alignment");
  static_assert(alignment >= alignof(T2), "insufficient alignment");
  static_assert(isPowerOfTwo(sizeof(T2)), "not power-of-two");

  return alignedMallocArray<T, alignment, doRoundUp>(nmemb, sizeof(T2));
}

// coverity[+free : arg-0]
inline void alignedFree(void* ptr) {
#if defined(HAVE_ALIGNED_MALLOC)
  _aligned_free(ptr);
#else
  free(ptr); // NOLINT
#endif
}

// coverity[+free : arg-0]
inline void alignedFreeConstPtr(const void* ptr) {
  // an exception, specified by EXP05-C-EX1 and EXP55-CPP-EX1
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast): this is fine.
  alignedFree(const_cast<void*>(ptr));
}

} // namespace rawspeed
