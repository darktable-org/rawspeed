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

#include "adt/Array1DRef.h"
#include "adt/Invariant.h"
#include <cstddef>
#include <optional>
#include <type_traits>
#include <vector>

namespace rawspeed {

template <class T> class Array2DRef final {
  T* _data = nullptr;
  int _pitch = 0;

  friend Array2DRef<const T>; // We need to be able to convert to const version.

  // We need to be able to convert to std::byte.
  friend Array2DRef<std::byte>;
  friend Array2DRef<const std::byte>;

public:
  using value_type = T;
  using cvless_value_type = std::remove_cv_t<value_type>;

  int width = 0;
  int height = 0;

  Array2DRef() = default;

  Array2DRef(T* data, int width, int height, int pitch = 0);

  // Can not cast away constness.
  template <typename T2>
    requires(std::is_const_v<T2> && !std::is_const_v<T>)
  Array2DRef(Array2DRef<T2> RHS) = delete;

  // Can not change type to non-byte.
  template <typename T2>
    requires(!(std::is_const_v<T2> && !std::is_const_v<T>) &&
             !std::is_same_v<std::remove_const_t<T>, std::remove_const_t<T2>> &&
             !std::is_same_v<std::remove_const_t<T>, std::byte>)
  Array2DRef(Array2DRef<T2> RHS) = delete;

  // Conversion from Array2DRef<T> to Array2DRef<const T>.
  template <typename T2>
    requires(!std::is_const_v<T2> && std::is_const_v<T> &&
             std::is_same_v<std::remove_const_t<T>, std::remove_const_t<T2>>)
  Array2DRef(Array2DRef<T2> RHS) // NOLINT google-explicit-constructor
      : _data(RHS._data), _pitch(RHS._pitch), width(RHS.width),
        height(RHS.height) {}

  // Const-preserving conversion from Array2DRef<T> to Array2DRef<std::byte>.
  template <typename T2>
    requires(!(std::is_const_v<T2> && !std::is_const_v<T>) &&
             !(std::is_same_v<std::remove_const_t<T>,
                              std::remove_const_t<T2>>) &&
             std::is_same_v<std::remove_const_t<T>, std::byte>)
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

  [[nodiscard]] std::optional<Array1DRef<T>> getAsArray1DRef() const;

  Array1DRef<T> operator[](int row) const;

  T& operator()(int row, int col) const;
};

// CTAD deduction guide
template <typename T>
explicit Array2DRef(T* data, int width, int height, int pitch = 0)
    -> Array2DRef<T>;

template <class T>
Array2DRef<T>::Array2DRef(T* data, const int width_, const int height_,
                          const int pitch_ /* = 0 */)
    : _data(data), _pitch(pitch_ == 0 ? width_ : pitch_), width(width_),
      height(height_) {
  invariant(width >= 0);
  invariant(height >= 0);
  invariant(_pitch >= 0);
  invariant(_pitch >= width);
}

template <class T>
[[nodiscard]] inline std::optional<Array1DRef<T>>
Array2DRef<T>::getAsArray1DRef() const {
  if (height == 1 || _pitch == width)
    return {{_data, width * height}};
  return std::nullopt;
}

template <class T>
inline Array1DRef<T> Array2DRef<T>::operator[](const int row) const {
  invariant(_data);
  invariant(row >= 0);
  invariant(row < height);
  invariant(_pitch >= width);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wunsafe-buffer-usage"
  return Array1DRef<T>(&_data[row * _pitch], width);
#pragma GCC diagnostic pop
}

template <class T>
inline T& Array2DRef<T>::operator()(const int row, const int col) const {
  invariant(col >= 0);
  invariant(col < width);
  return (operator[](row))(col);
}

} // namespace rawspeed
