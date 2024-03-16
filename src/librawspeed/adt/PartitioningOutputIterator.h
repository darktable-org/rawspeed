/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2024 Roman Lebedev

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

#include "adt/Bit.h"
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <type_traits>

namespace rawspeed {

template <typename UnderlyingOutputIterator,
          typename PartType =
              typename UnderlyingOutputIterator::container_type::value_type>
  requires(std::output_iterator<UnderlyingOutputIterator, uint8_t> &&
           std::unsigned_integral<PartType>)
class PartitioningOutputIterator {
  UnderlyingOutputIterator it;

public:
  using iterator_concept = std::output_iterator_tag;
  using value_type = void;
  using difference_type = ptrdiff_t;
  using pointer = void;
  using reference = void;

  PartitioningOutputIterator() = default;

  template <typename U>
    requires std::same_as<UnderlyingOutputIterator, std::remove_reference_t<U>>
  explicit PartitioningOutputIterator(U&& it_) : it(std::forward<U>(it_)) {}

  [[nodiscard]] PartitioningOutputIterator& operator*() { return *this; }

  PartitioningOutputIterator& operator++() { return *this; }

  // NOLINTNEXTLINE(cert-dcl21-cpp)
  PartitioningOutputIterator operator++(int) { return *this; }

  template <typename U>
    requires(std::unsigned_integral<U> && sizeof(U) >= sizeof(PartType) &&
             sizeof(U) % sizeof(PartType) == 0)
  PartitioningOutputIterator& operator=(U coalesced) {
    // NOLINTNEXTLINE(bugprone-sizeof-expression)
    constexpr int NumParts = sizeof(U) / sizeof(PartType);
    for (int i = 0; i != NumParts; ++i) {
      const auto part = static_cast<PartType>(coalesced);
      if constexpr (NumParts != 1)
        coalesced >>= bitwidth<PartType>();
      *it = part;
      ++it;
    }
    return *this;
  }
};

// CTAD deduction guide
template <typename T>
PartitioningOutputIterator(T) -> PartitioningOutputIterator<T>;

} // namespace rawspeed
