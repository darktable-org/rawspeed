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

#include "common/Memory.h"

#ifndef NDEBUG
#include "common/Common.h" // for isPowerOfTwo
#endif

#include <cassert> // for assert
#include <cstddef> // for size_t, uintptr_t

#if defined(HAVE_MM_MALLOC)
// for _mm_malloc, _mm_free
#include <xmmintrin.h> // IWYU pragma: keep
// IWYU pragma: no_include <mm_malloc.h>
#elif defined(HAVE_ALIGNED_MALLOC)
extern "C" {
#include <malloc.h> // for _aligned_malloc, _aligned_free
}
#else
#include <cstdlib> // for posix_memalign / aligned_alloc / malloc; free
#endif

namespace RawSpeed {

void* alignedMalloc(size_t size, size_t alignment) {
  assert(isPowerOfTwo(alignment)); // for posix_memalign, _aligned_malloc
  assert(((uintptr_t)alignment % sizeof(void*)) == 0); // for posix_memalign
  assert(((uintptr_t)size % alignment) == 0);          // for aligned_alloc

  void* ptr = nullptr;

#if defined(HAVE_POSIX_MEMALIGN)
  if (0 != posix_memalign(&ptr, alignment, size))
    return nullptr;
#elif defined(HAVE_ALIGNED_ALLOC)
  ptr = aligned_alloc(alignment, size);
#elif defined(HAVE_MM_MALLOC)
  ptr = _mm_malloc(size, alignment);
#elif defined(HAVE_ALIGNED_MALLOC)
  ptr = _aligned_malloc(size, alignment);
#else
#pragma message "No aligned malloc() implementation avaliable!"

#ifdef __APPLE__
  // apple malloc() aligns to 16 by default
  assert(alignment <= 16);
#endif

  ptr = malloc(size);
#endif

  assert(((uintptr_t)ptr % alignment) == 0);

  return ptr;
}

void alignedFree(void* ptr) {
#if defined(HAVE_MM_MALLOC)
  _mm_free(ptr);
#elif defined(HAVE_ALIGNED_MALLOC)
  _aligned_free(ptr);
#else
  free(ptr);
#endif
}

} // Namespace RawSpeed
