/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2022 Roman Lebedev

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

#include <utility>

namespace rawspeed {

template <typename Iter> class iterator_range {
  Iter begin_iterator;
  Iter end_iterator;

public:
  iterator_range(Iter begin_iterator_, Iter end_iterator_)
      : begin_iterator(std::move(begin_iterator_)),
        end_iterator(std::move(end_iterator_)) {}

  [[nodiscard]] Iter begin() const { return begin_iterator; }
  [[nodiscard]] Iter end() const { return end_iterator; }
  [[nodiscard]] bool empty() const { return begin_iterator == end_iterator; }
};

template <class T> iterator_range<T> make_range(T x, T y) {
  return iterator_range<T>(std::move(x), std::move(y));
}

template <typename T> iterator_range<T> make_range(std::pair<T, T> p) {
  return iterator_range<T>(std::move(p.first), std::move(p.second));
}

} // namespace rawspeed
