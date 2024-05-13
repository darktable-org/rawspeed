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

#include "adt/Bit.h"
#include "adt/Invariant.h"
#include <cstddef>
#include <iterator>
#include <type_traits>

namespace rawspeed {

template <typename T>
  requires std::is_unsigned_v<T>
struct BitMSBIterator final {
  T bitsPat;
  int bitIdx;

  using iterator_category = std::input_iterator_tag;
  using difference_type = std::ptrdiff_t;
  using value_type = bool;
  using pointer = const value_type*;   // Unusable, but must be here.
  using reference = const value_type&; // Unusable, but must be here.

  BitMSBIterator(T bitsPat_, int bitIdx_) : bitsPat(bitsPat_), bitIdx(bitIdx_) {
    invariant(bitIdx < static_cast<int>(bitwidth<T>()) && bitIdx >= -1);
  }

  value_type operator*() const {
    invariant(static_cast<unsigned>(bitIdx) < bitwidth<T>() &&
              "Iterator overflow");
    return (bitsPat >> bitIdx) & 0b1;
  }
  BitMSBIterator& operator++() {
    --bitIdx; // We go from MSB to LSB.
    invariant(bitIdx >= -1);
    return *this;
  }
  friend bool operator==(const BitMSBIterator& a, const BitMSBIterator& b) {
    invariant(a.bitsPat == b.bitsPat && "Comparing unrelated iterators.");
    return a.bitIdx == b.bitIdx;
  }
};

} // namespace rawspeed
