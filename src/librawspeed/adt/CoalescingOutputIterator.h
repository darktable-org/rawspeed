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
#include "adt/Invariant.h"
#include "io/Endianness.h"
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <type_traits>

namespace rawspeed {

template <typename UnderlyingOutputIterator, typename PartType = uint8_t,
          typename CoalescedType =
              typename UnderlyingOutputIterator::container_type::value_type>
  requires(std::output_iterator<UnderlyingOutputIterator, CoalescedType> &&
           std::unsigned_integral<CoalescedType> &&
           std::unsigned_integral<PartType> &&
           sizeof(PartType) <= sizeof(CoalescedType) &&
           sizeof(CoalescedType) % sizeof(PartType) == 0)
class CoalescingOutputIterator {
  UnderlyingOutputIterator it;

  CoalescedType cache = 0;
  int occupancy = 0;

  static constexpr int MaxOccupancy = bitwidth<CoalescedType>();

  void establishClassInvariants() const {
    invariant(occupancy >= 0);
    invariant(occupancy <= MaxOccupancy);
    invariant(occupancy % bitwidth<PartType>() == 0);
  }

  void maybeOutputCoalescedParts() {
    establishClassInvariants();
    invariant(occupancy > 0);
    if (occupancy != MaxOccupancy)
      return;
    *it = getLE<CoalescedType>(&cache);
    ++it;
    cache = 0;
    occupancy = 0;
  }

  struct DummyContainerType {
    using value_type = PartType;
  };

public:
  using iterator_concept = std::output_iterator_tag;
  using value_type = void;
  using difference_type = ptrdiff_t;
  using pointer = void;
  using reference = void;

  using container_type = DummyContainerType;

  CoalescingOutputIterator() = delete;

  template <typename U>
    requires std::same_as<UnderlyingOutputIterator, std::remove_reference_t<U>>
  explicit CoalescingOutputIterator(U&& it_) : it(std::forward<U>(it_)) {}

  CoalescingOutputIterator(const CoalescingOutputIterator& other)
      : it(other.it) {
    invariant(occupancy == 0);
    invariant(other.occupancy == 0);
  }

  // NOLINTBEGIN(performance-move-constructor-init)
  CoalescingOutputIterator(CoalescingOutputIterator&& other) noexcept
      : CoalescingOutputIterator(
            static_cast<const CoalescingOutputIterator&>(other)) {}
  // NOLINTEND(performance-move-constructor-init)

  CoalescingOutputIterator& operator=(const CoalescingOutputIterator& other) {
    invariant(occupancy == 0);
    invariant(other.occupancy == 0);
    CoalescingOutputIterator tmp(other);
    std::swap(*this, tmp);
    return *this;
  }
  CoalescingOutputIterator&
  operator=(CoalescingOutputIterator&& other) noexcept {
    *this = static_cast<const CoalescingOutputIterator&>(other);
    return *this;
  }

  ~CoalescingOutputIterator() {
    establishClassInvariants();
    if (occupancy == 0)
      return;
    int numPaddingBits = MaxOccupancy - occupancy;
    invariant(numPaddingBits > 0);
    invariant(numPaddingBits < MaxOccupancy);
    occupancy += numPaddingBits;
    invariant(occupancy == MaxOccupancy);
    maybeOutputCoalescedParts();
    invariant(occupancy == 0);
  }

  [[nodiscard]] CoalescingOutputIterator& operator*() {
    establishClassInvariants();
    return *this;
  }

  CoalescingOutputIterator& operator++() {
    establishClassInvariants();
    return *this;
  }

  // NOLINTNEXTLINE(cert-dcl21-cpp)
  CoalescingOutputIterator operator++(int) {
    establishClassInvariants();
    return *this;
  }

  template <typename U>
    requires std::same_as<U, PartType>
  CoalescingOutputIterator& operator=(U part) {
    establishClassInvariants();
    invariant(occupancy < MaxOccupancy);
    invariant(occupancy + bitwidth<U>() <= MaxOccupancy);
    static_assert(bitwidth<U>() <= MaxOccupancy);
    part = getLE<U>(&part);
    cache |= static_cast<CoalescedType>(part) << occupancy;
    occupancy += bitwidth<U>();
    invariant(occupancy <= MaxOccupancy);
    maybeOutputCoalescedParts();
    return *this;
  }
};

// CTAD deduction guide
template <typename T>
CoalescingOutputIterator(T) -> CoalescingOutputIterator<T>;

} // namespace rawspeed
