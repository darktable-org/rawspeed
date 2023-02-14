/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2023 Roman Lebedev

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

#include "AddressSanitizer.h"
#include "common/Common.h"
#include "common/Memory.h"
#include <cstddef> // for size_t
#include <memory>  // for allocator_traits

namespace rawspeed {

template <class T, int alignment> struct AlignedAllocator {
  using self = AlignedAllocator<T, alignment>;
  using allocator_traits = std::allocator_traits<self>;

public:
  using value_type = T;

  template <class U> struct rebind {
    using other = AlignedAllocator<U, alignment>;
  };

  T* allocate(std::size_t numElts) {
    assert(numElts > 0 && "Should not be trying to allocate no elements");
    assert(numElts <= allocator_traits::max_size(*this) &&
           "Can allocate this many elements.");
    assert(numElts <= SIZE_MAX / sizeof(T) &&
           "Byte count calculation will not overflow");

    std::size_t numBytes = sizeof(T) * numElts;
    std::size_t numPaddedBytes = roundUp(numBytes, alignment);
    assert(numPaddedBytes >= numBytes && "Alignment did not cause wraparound.");

    auto* r = alignedMalloc<T, alignment>(numPaddedBytes);
    if (!r)
      throw std::bad_alloc();
    ASan::PoisonMemoryRegion(
        reinterpret_cast<const volatile std::byte*>(r + numElts),
        numPaddedBytes - numBytes);
    return r;
  }

  void deallocate(T* p, std::size_t n) noexcept {
    assert(n > 0);
    alignedFree(p);
  }

  using propagate_on_container_copy_assignment = std::true_type;
  using propagate_on_container_move_assignment = std::true_type;
  using propagate_on_container_swap = std::true_type;
};

template <class T1, int A1, class T2, int A2>
bool operator==(const AlignedAllocator<T1, A1>& /*unused*/,
                const AlignedAllocator<T2, A2>& /*unused*/) {
  return A1 == A2;
}

template <class T1, int A1, class T2, int A2>
bool operator!=(const AlignedAllocator<T1, A1>& /*unused*/,
                const AlignedAllocator<T2, A2>& /*unused*/) {
  return !(A1 == A2);
}

} // namespace rawspeed
