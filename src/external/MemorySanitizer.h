/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2018 Roman Lebedev

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

#include "adt/CroppedArray1DRef.h" // for CroppedArray1DRef
#include "adt/CroppedArray2DRef.h" // for CroppedArray2DRef
#include <cstddef>                 // for byte, size_t

// see http://clang.llvm.org/docs/LanguageExtensions.html
#ifndef __has_feature      // Optional of course.
#define __has_feature(x) 0 // Compatibility with non-clang compilers.
#endif
#ifndef __has_extension
#define __has_extension __has_feature // Compatibility with pre-3.0 compilers.
#endif

#if __has_feature(memory_sanitizer) || defined(__SANITIZE_MEMORY__)
#include <sanitizer/msan_interface.h>
#endif

namespace rawspeed {

struct MSan final {
  // Do not instantiate.
  MSan() = delete;
  MSan(const MSan&) = delete;
  MSan(MSan&&) = delete;
  MSan& operator=(const MSan&) = delete;
  MSan& operator=(MSan&&) = delete;
  ~MSan() = delete;

  /* Declare memory chunk as being newly-allocated. */
  static void Allocated(const void* addr, size_t size);

  template <typename T> static void Allocated(const T& elt);

private:
  // Checks that memory range is fully initialized,
  // and reports an error if it
  static void CheckMemIsInitialized(const void* addr, size_t size);
  static void CheckMemIsInitialized(CroppedArray1DRef<std::byte> row);

public:
  // Checks that memory range is fully initialized,
  // and reports an error if it
  static void CheckMemIsInitialized(CroppedArray2DRef<std::byte> frame);
};

#if __has_feature(memory_sanitizer) || defined(__SANITIZE_MEMORY__)
inline void MSan::Allocated(const void* addr, size_t size) {
  __msan_allocated_memory(addr, size);
}
#else
inline void MSan::Allocated([[maybe_unused]] const void* addr,
                            [[maybe_unused]] size_t size) {
  // If we are building without MSAN, then there is no way to have a non-empty
  // body of this function. It's better than to have a macros, or to use
  // preprocessor in every place it is called.
}
#endif

template <typename T> inline void MSan::Allocated(const T& elt) {
  Allocated(&elt, sizeof(T));
}

#if __has_feature(memory_sanitizer) || defined(__SANITIZE_MEMORY__)
inline void MSan::CheckMemIsInitialized(const void* addr, size_t size) {
  __msan_check_mem_is_initialized(addr, size);
}
#else
inline void MSan::CheckMemIsInitialized([[maybe_unused]] const void* addr,
                                        [[maybe_unused]] size_t size) {
  // If we are building without MSAN, then there is no way to have a non-empty
  // body of this function. It's better than to have a macros, or to use
  // preprocessor in every place it is called.
}
#endif

inline void MSan::CheckMemIsInitialized(CroppedArray1DRef<std::byte> row) {
  MSan::CheckMemIsInitialized(row.begin(), row.size());
}
inline void MSan::CheckMemIsInitialized(CroppedArray2DRef<std::byte> frame) {
  for (int row = 0; row < frame.croppedHeight; row++)
    CheckMemIsInitialized(frame[row]);
}

} // namespace rawspeed
