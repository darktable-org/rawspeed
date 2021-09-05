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

template <typename T> struct RangesSortCmp final {
	constexpr bool operator()(const T& lhs, const T& rhs) const {
		return lhs.begin() < rhs.begin();
	}
};

template <typename T> class NORangesSet : public std::set<T, RangesSortCmp<T>>
{
public:
	std::pair<typename std::set<T, RangesSortCmp<T>>::iterator, bool> emplace(const T& a)
    {
        for (const auto& t : *this)
        {
            if (RangesOverlap(t, a))
                return std::make_pair(this->end(), false);
            if (t.begin() > a.end()) // std::set is sorted by t.begin(), so if t is completely on the right we may break comparison loop
                break;
        }
        return std::set<T, RangesSortCmp<T>>::emplace(a);
    }
};

} // namespace rawspeed
