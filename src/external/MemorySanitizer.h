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

#include <cstddef> // for byte, size_t

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

  /* Checks that memory range is fully initialized, and reports an error if it
   * is not. */
  static void CheckMemIsInitialized(const std::byte* addr, size_t size);
};

#if __has_feature(memory_sanitizer) || defined(__SANITIZE_MEMORY__)
inline void MSan::CheckMemIsInitialized(const std::byte* addr, size_t size) {
  __msan_check_mem_is_initialized(addr, size);
}
#else
inline void MSan::CheckMemIsInitialized([[maybe_unused]] const std::byte* addr,
                                        [[maybe_unused]] size_t size) {
  // If we are building without MSAN, then there is no way to have a non-empty
  // body of this function. It's better than to have a macros, or to use
  // preprocessor in every place it is called.
}
#endif

} // namespace rawspeed
