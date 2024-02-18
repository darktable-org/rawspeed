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

#include "rawspeedconfig.h"
#include <type_traits>

namespace rawspeed {

template <typename Ttgt, typename Tsrc>
  requires(((std::is_integral_v<Tsrc> || std::is_floating_point_v<Tsrc>) &&
            (std::is_integral_v<Ttgt> || std::is_floating_point_v<Ttgt>)) &&
           !std::is_same_v<Tsrc, Ttgt>)
constexpr RAWSPEED_READNONE Ttgt implicit_cast(Tsrc value) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wdouble-promotion"
  return value;
#pragma GCC diagnostic pop
}

// Sometimes through templates in some template instantiations
// the types do end up being the same. This is fine.
template <typename Ttgt, typename Tsrc>
  requires(((std::is_integral_v<Tsrc> || std::is_floating_point_v<Tsrc>) &&
            (std::is_integral_v<Ttgt> || std::is_floating_point_v<Ttgt>)) &&
           std::is_same_v<Tsrc, Ttgt>)
constexpr RAWSPEED_READNONE Ttgt implicit_cast(Tsrc value) {
  return value;
}

} // namespace rawspeed
