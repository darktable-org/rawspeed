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

#include "common/Range.h" // for operator<, RangesOverlap
#include <algorithm>      // for partition_point
#include <cassert>        // for assert
#include <cstddef>        // for size_t
#include <iterator>       // for prev
#include <set>            // IWYU pragma: export
#include <utility>        // for pair

namespace rawspeed {

template <typename T> class NORangesSet final {
  std::set<T> elts;

  [[nodiscard]] bool
  rangeIsOverlappingExistingElementOfSortedSet(const T& newElt) const {
    // If there are no elements in set, then the new element
    // does not overlap any existing elements.
    if (elts.empty())
      return false;

    // Find the first element that is not less than the new element.
    auto p =
        std::partition_point(elts.begin(), elts.end(),
                             [newElt](const T& elt) { return elt < newElt; });

    // If there is such an element, we must not overlap with it.
    if (p != elts.end() && RangesOverlap(newElt, *p))
      return true;

    // Now, is there an element before the element we found?
    if (p == elts.begin())
      return false;

    // There is. We *also* must not overlap with that element too.
    auto prevBeforeP = std::prev(p);
    return RangesOverlap(newElt, *prevBeforeP);
  }

public:
  bool insert(const T& newElt) {
    if (rangeIsOverlappingExistingElementOfSortedSet(newElt))
      return false;

    auto i = elts.insert(newElt);
    assert(i.second && "Did not insert after all?");
    (void)i;
    return true;
  }

  [[nodiscard]] std::size_t size() const { return elts.size(); }
};

} // namespace rawspeed
