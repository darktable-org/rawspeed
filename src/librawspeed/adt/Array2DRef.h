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

#include <cassert>     // for assert
#include <cstddef>     // for byte
#include <type_traits> // for negation, is_const, remove_const_t, is_same
#include <vector>      // for vector

namespace rawspeed {

template <class T> class Array2DRef {
  T* _data = nullptr;
  int _pitch = 0;

  friend Array2DRef<const T>; // We need to be able to convert to const version.

  // We need to be able to convert to std::byte.
  friend Array2DRef<std::byte>;
  friend Array2DRef<const std::byte>;

  T& operator[](int row) const;

public:
  using value_type = T;
  using cvless_value_type = std::remove_cv_t<value_type>;

  int width = 0;
  int height = 0;

  Array2DRef() = default;

  Array2DRef(T* data, int dataWidth, int dataHeight, int dataPitch = 0);

  // Can not cast away constness.
  template <
      typename T2,
      std::enable_if_t<std::conjunction_v<std::is_const<T2>,
                                          std::negation<std::is_const<T>>>,
                       bool> = true>
  Array2DRef(Array2DRef<T2> RHS) = delete;

  // Can not change type to non-byte.
  template <
      typename T2,
      std::enable_if_t<
          std::conjunction_v<
              std::negation<std::conjunction<std::is_const<T2>,
                                             std::negation<std::is_const<T>>>>,
              std::negation<std::is_same<std::remove_const_t<T>,
                                         std::remove_const_t<T2>>>,
              std::negation<std::is_same<std::remove_const_t<T>, std::byte>>>,
          bool> = true>
  Array2DRef(Array2DRef<T2> RHS) = delete;

  // Conversion from Array2DRef<T> to Array2DRef<const T>.
  template <
      typename T2,
      std::enable_if_t<
          std::conjunction_v<
              std::conjunction<std::negation<std::is_const<T2>>,
                               std::is_const<T>>,
              std::is_same<std::remove_const_t<T>, std::remove_const_t<T2>>>,
          bool> = true>
  Array2DRef(Array2DRef<T2> RHS) // NOLINT google-explicit-constructor
      : _data(RHS._data), _pitch(RHS._pitch), width(RHS.width),
        height(RHS.height) {}

  // Const-preserving conversion from Array2DRef<T> to Array2DRef<std::byte>.
  template <typename T2,
            std::enable_if_t<
                std::conjunction_v<
                    std::negation<std::conjunction<
                        std::is_const<T2>, std::negation<std::is_const<T>>>>,
                    std::negation<std::is_same<std::remove_const_t<T>,
                                               std::remove_const_t<T2>>>,
                    std::is_same<std::remove_const_t<T>, std::byte>>,
                bool> = true>
  Array2DRef(Array2DRef<T2> RHS) // NOLINT google-explicit-constructor
      : _data(reinterpret_cast<T*>(RHS._data)), _pitch(sizeof(T2) * RHS._pitch),
        width(sizeof(T2) * RHS.width), height(RHS.height) {}

  template <typename AllocatorType =
                typename std::vector<cvless_value_type>::allocator_type>
  static Array2DRef<T>
  create(std::vector<cvless_value_type, AllocatorType>& storage, int width,
         int height) {
    using VectorTy = std::remove_reference_t<decltype(storage)>;
    storage = VectorTy(width * height);
    return {storage.data(), width, height};
  }

  T& operator()(int row, int col) const;
};

template <class T>
Array2DRef<T>::Array2DRef(T* data, const int dataWidth, const int dataHeight,
                          const int dataPitch /* = 0 */)
    : _data(data), width(dataWidth), height(dataHeight) {
  assert(width >= 0);
  assert(height >= 0);
  _pitch = (dataPitch == 0 ? dataWidth : dataPitch);
}

template <class T> inline T& Array2DRef<T>::operator[](const int row) const {
  assert(_data);
  assert(row >= 0);
  assert(row < height);
  return _data[row * _pitch];
}

template <class T>
inline T& Array2DRef<T>::operator()(const int row, const int col) const {
  assert(col >= 0);
  assert(col < width);
  return (&(operator[](row)))[col];
}

} // namespace rawspeed
