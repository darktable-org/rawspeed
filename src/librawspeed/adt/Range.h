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
#include <algorithm>
#include <cassert>
#include <iterator>
#include <type_traits>
#include <utility>

namespace rawspeed {

template <typename T> class Range final {
  T base;
  std::make_unsigned_t<T> size;

public:
  constexpr Range() = default;

  template <typename T2>
    requires std::is_unsigned_v<T2>
  constexpr Range(T base_, T2 size_) : base(base_), size(size_) {}

  constexpr T RAWSPEED_READNONE begin() const { return base; }

  constexpr T RAWSPEED_READNONE end() const { return base + T(size); }
};

template <typename T> bool operator<(const Range<T>& lhs, const Range<T>& rhs) {
  return std::pair(lhs.begin(), lhs.end()) < std::pair(rhs.begin(), rhs.end());
}

template <typename Tr, typename Tv>
constexpr bool RAWSPEED_READNONE RangeContains(const Tr& r, Tv pos) {
  if (pos < std::begin(r))
    return false;

  assert(pos >= std::begin(r));
  return std::end(r) > pos;
}

template <typename T>
constexpr bool RAWSPEED_READNONE RangesOverlap(const T& lhs, const T& rhs) {
  if (&lhs == &rhs)
    return true;

  if (std::begin(lhs) == std::begin(rhs))
    return true;

  const std::pair<const T&, const T&> ordered =
      std::minmax(lhs, rhs, [](const T& r0, const T& r1) {
        assert(std::begin(r0) != std::begin(r1));
        return std::begin(r0) < std::begin(r1);
      });

  assert(std::begin(ordered.first) < std::begin(ordered.second));
  return RangeContains(ordered.first, std::begin(ordered.second));
}

} // namespace rawspeed
