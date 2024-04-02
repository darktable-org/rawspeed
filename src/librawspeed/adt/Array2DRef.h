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

#include "rawspeedconfig.h"
#include "adt/Array1DRef.h"
#include "adt/CroppedArray1DRef.h" // IWYU pragma: keep
#include "adt/Invariant.h"
#include "adt/Optional.h"
#include <cstddef>
#include <type_traits>
#include <vector>

namespace rawspeed {

template <class T> class Array2DRef final {
  Array1DRef<T> data;
  int _pitch;

  int _width;
  int _height;

  friend Array2DRef<const T>; // We need to be able to convert to const version.

  // We need to be able to convert to std::byte.
  friend Array2DRef<std::byte>;
  friend Array2DRef<const std::byte>;

public:
  void establishClassInvariants() const noexcept;

  Array2DRef(Array1DRef<T> data, int width, int height, int pitch);

  using value_type = T;
  using cvless_value_type = std::remove_cv_t<value_type>;

  [[nodiscard]] int RAWSPEED_READONLY pitch() const;
  [[nodiscard]] int RAWSPEED_READONLY width() const;
  [[nodiscard]] int RAWSPEED_READONLY height() const;

  Array2DRef() = delete;

  Array2DRef(T* data, int width, int height, int pitch);

  Array2DRef(T* data, int width, int height);

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
  Array2DRef(Array2DRef<T2> RHS) // NOLINT(google-explicit-constructor)
      : Array2DRef(RHS.data, RHS._width, RHS._height, RHS._pitch) {}

  // Const-preserving conversion from Array2DRef<T> to Array2DRef<std::byte>.
  template <typename T2>
    requires(
        !(std::is_const_v<T2> && !std::is_const_v<T>) &&
        !(std::is_same_v<std::remove_const_t<T>, std::remove_const_t<T2>>) &&
        std::is_same_v<std::remove_const_t<T>, std::byte>)
  Array2DRef(Array2DRef<T2> RHS) // NOLINT(google-explicit-constructor)
      : Array2DRef(RHS.data, sizeof(T2) * RHS._width, RHS._height,
                   sizeof(T2) * RHS._pitch) {}

  template <typename AllocatorType =
                typename std::vector<cvless_value_type>::allocator_type>
  static Array2DRef<T>
  create(std::vector<cvless_value_type, AllocatorType>& storage, int width,
         int height) {
    using VectorTy = std::remove_reference_t<decltype(storage)>;
    storage = VectorTy(width * height);
    return {storage.data(), width, height};
  }

  [[nodiscard]] Optional<Array1DRef<T>> getAsArray1DRef() const;

  Array1DRef<T> operator[](int row) const;

  T& operator()(int row, int col) const;
};

// CTAD deduction guide
template <typename T>
explicit Array2DRef(Array1DRef<T> data, int width, int height,
                    int pitch) -> Array2DRef<T>;

// CTAD deduction guide
template <typename T>
explicit Array2DRef(T* data, int width, int height, int pitch) -> Array2DRef<T>;

// CTAD deduction guide
template <typename T>
explicit Array2DRef(T* data, int width, int height) -> Array2DRef<T>;

template <class T>
__attribute__((always_inline)) inline void
Array2DRef<T>::establishClassInvariants() const noexcept {
  data.establishClassInvariants();
  invariant(_width >= 0);
  invariant(_height >= 0);
  invariant(_pitch != 0);
  invariant(_pitch >= 0);
  invariant(_pitch >= _width);
  invariant((_width == 0) == (_height == 0));
  invariant(data.size() == _pitch * _height);
}

template <class T>
inline Array2DRef<T>::Array2DRef(Array1DRef<T> data_, const int width_,
                                 const int height_, const int pitch_)
    : data(data_), _pitch(pitch_), _width(width_), _height(height_) {
  establishClassInvariants();
}

template <class T>
inline Array2DRef<T>::Array2DRef(T* data_, const int width_, const int height_,
                                 const int pitch_)
    : Array2DRef({data_, pitch_ * height_}, width_, height_, pitch_) {
  establishClassInvariants();
}

template <class T>
inline Array2DRef<T>::Array2DRef(T* data_, const int width_, const int height_)
    : Array2DRef(data_, width_, height_, /*pitch=*/width_) {
  establishClassInvariants();
}

template <class T>
__attribute__((always_inline)) inline int Array2DRef<T>::pitch() const {
  establishClassInvariants();
  return _pitch;
}

template <class T>
__attribute__((always_inline)) inline int Array2DRef<T>::width() const {
  establishClassInvariants();
  return _width;
}

template <class T>
__attribute__((always_inline)) inline int Array2DRef<T>::height() const {
  establishClassInvariants();
  return _height;
}

template <class T>
[[nodiscard]] inline Optional<Array1DRef<T>>
Array2DRef<T>::getAsArray1DRef() const {
  establishClassInvariants();
  if (height() == 1 || _pitch == width())
    return data.getCrop(/*offset=*/0, width() * height()).getAsArray1DRef();
  return std::nullopt;
}

template <class T>
inline Array1DRef<T> Array2DRef<T>::operator[](const int row) const {
  establishClassInvariants();
  invariant(row >= 0);
  invariant(row < height());
  return data.getCrop(row * _pitch, width()).getAsArray1DRef();
}

template <class T>
__attribute__((always_inline)) inline T&
Array2DRef<T>::operator()(const int row, const int col) const {
  establishClassInvariants();
  invariant(col >= 0);
  invariant(col < width());
  return (operator[](row))(col);
}

} // namespace rawspeed
