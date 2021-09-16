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

#include <algorithm>   // for min
#include <cassert>     // for assert
#include <type_traits> // for enable_if, is_pointer
#include <utility>     // for pair

namespace rawspeed {

template <typename T> class Range final {
  T base;
  std::make_unsigned_t<T> size;

public:
  constexpr Range() = default;

  template <typename T2, typename = std::enable_if_t<std::is_unsigned_v<T2>>>
  constexpr Range(T base_, T2 size_) : base(base_), size(size_) {}

  constexpr T __attribute__((const)) begin() const { return base; }

  constexpr T __attribute__((const)) end() const { return base + T(size); }
};

template <typename T> bool operator<(const Range<T>& lhs, const Range<T>& rhs) {
  return std::pair(lhs.begin(), lhs.end()) < std::pair(rhs.begin(), rhs.end());
}

template <typename Tr, typename Tv>
constexpr bool __attribute__((const)) RangeContains(const Tr& r, Tv pos) {
  if (pos < r.begin())
    return false;

  assert(pos >= r.begin());
  return r.end() > pos;
}

template <typename T>
constexpr bool __attribute__((const))
RangesOverlap(const T& lhs, const T& rhs) {
  if (&lhs == &rhs)
    return true;

  if (lhs.begin() == rhs.begin())
    return true;

  const std::pair<const T&, const T&> ordered =
      std::minmax(lhs, rhs, [](const T& r0, const T& r1) {
        assert(r0.begin() != r1.begin());
        return r0.begin() < r1.begin();
      });

  assert(ordered.first.begin() < ordered.second.begin());
  return RangeContains(ordered.first, ordered.second.begin());
}

} // namespace rawspeed
