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
  unsigned int _pitch = 0;
  T* _data = nullptr;

  friend Array2DRef<const T>; // We need to be able to convert to const version.

public:
  unsigned int width = 0, height = 0;

  Array2DRef() = default;

  Array2DRef(T* data, unsigned int dataWidth, unsigned int dataHeight,
             unsigned int dataPitch = 0);

  // Conversion from Array2DRef<T> to Array2DRef<const T>.
  template <class T2, typename = std::enable_if_t<std::is_same<
                          typename std::remove_const<T>::type, T2>::value>>
  Array2DRef(Array2DRef<T2> RHS) { // NOLINT google-explicit-constructor
    _data = RHS._data;
    _pitch = RHS._pitch;
    width = RHS.width;
    height = RHS.height;
  }

  static std::vector<T> create(unsigned int width, unsigned int height);

  inline T& operator()(unsigned int x, unsigned int y) const;
};

template <class T>
Array2DRef<T>::Array2DRef(T* data, const unsigned int dataWidth,
                          const unsigned int dataHeight,
                          const unsigned int dataPitch /* = 0 */)
    : _data(data), width(dataWidth), height(dataHeight) {
  _pitch = (dataPitch == 0 ? dataWidth : dataPitch);
}

// static
template <class T>
std::vector<T> Array2DRef<T>::create(const unsigned int width,
                                     const unsigned int height) {
  std::vector<T> data;
  data.resize(width * height);
  return data;
}

template <class T>
T& Array2DRef<T>::operator()(const unsigned int x, const unsigned int y) const {
  assert(_data);
  assert(x < width);
  assert(y < height);
  return _data[y * _pitch + x];
}

} // namespace rawspeed
