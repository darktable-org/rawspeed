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

#include "common/Range.h" // for RangesOverlap
#include <set>            // IWYU pragma: export
// IWYU pragma: no_include <bits/stl_set.h>

namespace rawspeed {

template <typename T> struct RangesOverlapCmp final {
  constexpr bool operator()(const T& lhs, const T& rhs) const {
    return !RangesOverlap(lhs, rhs);
  }
};

template <typename T> using NORangesSet = std::set<T, RangesOverlapCmp<T>>;

} // namespace rawspeed
