/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2018 Stefan Löffler
    Copyright (C) 2018 Roman Lebedev

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

#include <cassert> // for assert
#include <type_traits>
#include <vector> // for vector

namespace rawspeed {

template <class T> class Array2DRef {
  int _pitch = 0;
  T* _data = nullptr;

  friend Array2DRef<const T>; // We need to be able to convert to const version.

  inline T& operator[](int row) const;

public:
  using value_type = T;
  using cvless_value_type = typename std::remove_cv<value_type>::type;

  int width = 0, height = 0;

  Array2DRef() = default;

  Array2DRef(T* data, int dataWidth, int dataHeight, int dataPitch = 0);

  // Conversion from Array2DRef<T> to Array2DRef<const T>.
  template <class T2, typename = std::enable_if_t<std::is_same<
                          typename std::remove_const<T>::type, T2>::value>>
  Array2DRef(Array2DRef<T2> RHS) { // NOLINT google-explicit-constructor
    _data = RHS._data;
    _pitch = RHS._pitch;
    width = RHS.width;
    height = RHS.height;
  }

  template <typename AllocatorType =
                typename std::vector<cvless_value_type>::allocator_type>
  static Array2DRef<T>
  create(std::vector<cvless_value_type, AllocatorType>* storage, int width,
         int height) {
    storage->resize(width * height);
    return {storage->data(), width, height};
  }

  inline T& operator()(int row, int col) const;
};

template <class T>
Array2DRef<T>::Array2DRef(T* data, const int dataWidth, const int dataHeight,
                          const int dataPitch /* = 0 */)
    : _data(data), width(dataWidth), height(dataHeight) {
  assert(width >= 0);
  assert(height >= 0);
  _pitch = (dataPitch == 0 ? dataWidth : dataPitch);
}

template <class T> T& Array2DRef<T>::operator[](const int row) const {
  assert(_data);
  assert(row >= 0);
  assert(row < height);
  return _data[row * _pitch];
}

template <class T>
T& Array2DRef<T>::operator()(const int row, const int col) const {
  assert(col >= 0);
  assert(col < width);
  return (&(operator[](row)))[col];
}

} // namespace rawspeed
