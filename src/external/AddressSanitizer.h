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

#include <cstddef> // for size_t

// see http://clang.llvm.org/docs/LanguageExtensions.html
#ifndef __has_feature      // Optional of course.
#define __has_feature(x) 0 // Compatibility with non-clang compilers.
#endif
#ifndef __has_extension
#define __has_extension __has_feature // Compatibility with pre-3.0 compilers.
#endif

#if __has_feature(address_sanitizer) || defined(__SANITIZE_ADDRESS__)
#include <sanitizer/asan_interface.h>
#endif

namespace rawspeed {

struct ASan final {
  // Do not instantiate.
  ASan() = delete;
  ASan(const ASan&) = delete;
  ASan(ASan&&) = delete;
  ASan& operator=(const ASan&) = delete;
  ASan& operator=(ASan&&) = delete;
  ~ASan() = delete;

  // Marks memory region [addr, addr+size) as unaddressable.
  static void PoisonMemoryRegion(void const volatile* addr, size_t size);
  // Marks memory region [addr, addr+size) as addressable.
  static void UnPoisonMemoryRegion(void const volatile* addr, size_t size);

  // If at least one byte in [beg, beg+size) is poisoned, return true
  // Otherwise return 0.
  static bool RegionIsPoisoned(void const volatile* addr, size_t size);
};

#if __has_feature(address_sanitizer) || defined(__SANITIZE_ADDRESS__)
inline void ASan::PoisonMemoryRegion(void const volatile* addr, size_t size) {
  __asan_poison_memory_region(addr, size);
}
inline void ASan::UnPoisonMemoryRegion(void const volatile* addr, size_t size) {
  __asan_unpoison_memory_region(addr, size);
}
inline bool ASan::RegionIsPoisoned(void const volatile* addr, size_t size) {
  auto* beg = const_cast<void*>(addr); // NOLINT
  return nullptr != __asan_region_is_poisoned(beg, size);
}
#else
inline void ASan::PoisonMemoryRegion(void const volatile* addr, size_t size) {
  // If we are building without ASan, then there is no way to have a non-empty
  // body of this function. It's better than to have a macros, or to use
  // preprocessor in every place it is called.
}
inline void ASan::UnPoisonMemoryRegion(void const volatile* addr, size_t size) {
  // If we are building without ASan, then there is no way to have a non-empty
  // body of this function. It's better than to have a macros, or to use
  // preprocessor in every place it is called.
}
inline bool ASan::RegionIsPoisoned(void const volatile* addr, size_t size) {
  // If we are building without ASan, then there is no way to have a poisoned
  // memory region.
  return false;
}
#endif

} // namespace rawspeed
