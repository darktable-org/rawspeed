/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2023-2024 Roman Lebedev

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
#include "adt/Invariant.h"
#include <bit>
#include <concepts>
#include <cstdint>
#include <type_traits>

namespace rawspeed {

namespace impl {

template <typename T0, typename T1>
  requires(std::is_same_v<T0, T1> && std::integral<T0>)
constexpr RAWSPEED_READNONE bool is_bitwise_identical(const T0& v0,
                                                      const T1& v1) {
  return v0 == v1;
}

template <typename T> struct bag_of_bits_type;
template <> struct bag_of_bits_type<float> {
  using value_type = uint32_t;
};
template <> struct bag_of_bits_type<double> {
  using value_type = uint64_t;
};

template <typename T0, typename T1>
  requires(std::is_same_v<T0, T1> && std::floating_point<T0>)
constexpr RAWSPEED_READNONE bool is_bitwise_identical(const T0& v0,
                                                      const T1& v1) {
  using Ti = typename bag_of_bits_type<T0>::value_type;
  return std::bit_cast<Ti>(v0) == std::bit_cast<Ti>(v1);
}

} // namespace impl

template <typename Ttgt, typename Tsrc>
  requires(((std::is_integral_v<Tsrc> || std::is_floating_point_v<Tsrc>) &&
            (std::is_integral_v<Ttgt> || std::is_floating_point_v<Ttgt>)) &&
           !std::is_same_v<Tsrc, Ttgt>)
constexpr RAWSPEED_READNONE Ttgt lossless_cast(Tsrc value) {
  const auto newValue = static_cast<Ttgt>(value);
  const auto roundTrippedValue = static_cast<Tsrc>(newValue);
  invariant(impl::is_bitwise_identical(roundTrippedValue, value));
  return newValue;
}

// Sometimes through templates in some template instantiations
// the types do end up being the same. This is fine.
template <typename Ttgt, typename Tsrc>
  requires(((std::is_integral_v<Tsrc> || std::is_floating_point_v<Tsrc>) &&
            (std::is_integral_v<Ttgt> || std::is_floating_point_v<Ttgt>)) &&
           std::is_same_v<Tsrc, Ttgt>)
constexpr RAWSPEED_READNONE Ttgt lossless_cast(Tsrc value) {
  return value;
}

template <typename Ttgt, typename Tsrc>
  requires(((std::is_integral_v<Tsrc> || std::is_floating_point_v<Tsrc>) &&
            (std::is_integral_v<Ttgt> || std::is_floating_point_v<Ttgt>)) &&
           !std::is_same_v<Tsrc, Ttgt>)
constexpr RAWSPEED_READNONE Ttgt lossy_cast(Tsrc value) {
  return static_cast<Ttgt>(value);
}

// Sometimes through templates in some template instantiations
// the types do end up being the same. This is fine.
template <typename Ttgt, typename Tsrc>
  requires(((std::is_integral_v<Tsrc> || std::is_floating_point_v<Tsrc>) &&
            (std::is_integral_v<Ttgt> || std::is_floating_point_v<Ttgt>)) &&
           std::is_same_v<Tsrc, Ttgt>)
constexpr RAWSPEED_READNONE Ttgt lossy_cast(Tsrc value) {
  return value;
}

} // namespace rawspeed
