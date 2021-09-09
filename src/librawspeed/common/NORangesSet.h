/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2017-2021 Roman Lebedev

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
#include <utility>        // for pair
// IWYU pragma: no_include <bits/stl_set.h>

namespace rawspeed {

template <typename T> class NORangesSet final {
  std::set<T> elts;

public:
  bool insert(const T& newElt) {
    if (std::any_of(elts.begin(), elts.end(),
                    [newElt](const auto& existingElt) {
                      return RangesOverlap(newElt, existingElt);
                    }))
      return false;
    elts.insert(newElt);
    return true;
  }

  [[nodiscard]] std::size_t size() const { return elts.size(); }
};

} // namespace rawspeed
