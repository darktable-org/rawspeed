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
#include "adt/Invariant.h"
#include <cstddef>
#include <type_traits>

namespace rawspeed {

template <class T> class CroppedArray1DRef;

template <class T> class Array1DRef final {
  T* data;
  int numElts;

  friend Array1DRef<const T>; // We need to be able to convert to const version.

  // We need to be able to convert to std::byte.
  friend Array1DRef<std::byte>;
  friend Array1DRef<const std::byte>;

public:
  void establishClassInvariants() const noexcept;

  using value_type = T;
  using cvless_value_type = std::remove_cv_t<value_type>;

  Array1DRef() = delete;

  Array1DRef(T* data, int numElts);

  // Can not cast away constness.
  template <typename T2>
    requires(std::is_const_v<T2> && !std::is_const_v<T>)
  Array1DRef(Array1DRef<T2> RHS) = delete;

  // Can not change type to non-byte.
  template <typename T2>
    requires(!(std::is_const_v<T2> && !std::is_const_v<T>) &&
             !std::is_same_v<std::remove_const_t<T>, std::remove_const_t<T2>> &&
             !std::is_same_v<std::remove_const_t<T>, std::byte>)
  Array1DRef(Array1DRef<T2> RHS) = delete;

  // Conversion from Array1DRef<T> to Array1DRef<const T>.
  template <typename T2>
    requires(!std::is_const_v<T2> && std::is_const_v<T> &&
             std::is_same_v<std::remove_const_t<T>, std::remove_const_t<T2>>)
  Array1DRef(Array1DRef<T2> RHS) // NOLINT(google-explicit-constructor)
      : Array1DRef(RHS.data, RHS.numElts) {}

  // Const-preserving conversion from Array1DRef<T> to Array1DRef<std::byte>.
  template <typename T2>
    requires(
        !(std::is_const_v<T2> && !std::is_const_v<T>) &&
        !(std::is_same_v<std::remove_const_t<T>, std::remove_const_t<T2>>) &&
        std::is_same_v<std::remove_const_t<T>, std::byte>)
  Array1DRef(Array1DRef<T2> RHS) // NOLINT(google-explicit-constructor)
      : Array1DRef(reinterpret_cast<T*>(RHS.data), sizeof(T2) * RHS.numElts) {}

  [[nodiscard]] CroppedArray1DRef<T> getCrop(int offset, int numElts) const;
  [[nodiscard]] CroppedArray1DRef<T> getBlock(int numElts, int index) const;

  [[nodiscard]] int RAWSPEED_READONLY size() const;

  [[nodiscard]] T* addressOf(int eltIdx) const;
  [[nodiscard]] T& operator()(int eltIdx) const;

  [[nodiscard]] T* begin() const;
  [[nodiscard]] T* end() const;
};

// CTAD deduction guide
template <typename T> Array1DRef(T* data_, int numElts_) -> Array1DRef<T>;

template <class T>
__attribute__((always_inline)) inline void
Array1DRef<T>::establishClassInvariants() const noexcept {
  invariant(data);
  invariant(numElts >= 0);
}

template <class T>
inline Array1DRef<T>::Array1DRef(T* data_, const int numElts_)
    : data(data_), numElts(numElts_) {
  establishClassInvariants();
}

template <class T>
[[nodiscard]] inline CroppedArray1DRef<T>
Array1DRef<T>::getCrop(int offset, int size) const {
  establishClassInvariants();
  invariant(offset >= 0);
  invariant(size >= 0);
  invariant(offset <= numElts);
  invariant(size <= numElts);
  invariant(offset + size <= numElts);
  return {*this, offset, size};
}

template <class T>
[[nodiscard]] inline CroppedArray1DRef<T>
Array1DRef<T>::getBlock(int size, int index) const {
  establishClassInvariants();
  invariant(index >= 0);
  invariant(size >= 0);
  invariant(index <= numElts);
  invariant(size <= numElts);
  return getCrop(size * index, size);
}

template <class T>
__attribute__((always_inline)) inline T*
Array1DRef<T>::addressOf(const int eltIdx) const {
  establishClassInvariants();
  invariant(eltIdx >= 0);
  invariant(eltIdx <= numElts);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wunsafe-buffer-usage"
  return data + eltIdx;
#pragma GCC diagnostic pop
}

template <class T>
__attribute__((always_inline)) inline T&
Array1DRef<T>::operator()(const int eltIdx) const {
  establishClassInvariants();
  invariant(eltIdx >= 0);
  invariant(eltIdx < numElts);
  return *addressOf(eltIdx);
}

template <class T> inline int Array1DRef<T>::size() const {
  establishClassInvariants();
  return numElts;
}

template <class T> inline T* Array1DRef<T>::begin() const {
  establishClassInvariants();
  return addressOf(/*eltIdx=*/0);
}
template <class T> inline T* Array1DRef<T>::end() const {
  establishClassInvariants();
  return addressOf(/*eltIdx=*/numElts);
}

} // namespace rawspeed
