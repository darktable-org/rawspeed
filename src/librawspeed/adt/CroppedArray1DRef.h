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
#include "adt/Array1DRef.h"
#include "adt/Invariant.h"
#include <type_traits>

namespace rawspeed {

template <class T> class CroppedArray1DRef final {
  Array1DRef<T> base;
  int offset;
  int numElts;

  friend CroppedArray1DRef<const T>; // We need to be able to convert to const
                                     // version.

  // Only allow construction from `Array1DRef<T>::getCrop()`.
  friend CroppedArray1DRef<T> Array1DRef<T>::getCrop(int offset,
                                                     int numElts) const;

  CroppedArray1DRef(Array1DRef<T> base, int offset, int numElts);

public:
  void establishClassInvariants() const noexcept;

  using value_type = T;
  using cvless_value_type = std::remove_cv_t<value_type>;

  CroppedArray1DRef() = delete;

  // Can not cast away constness.
  template <typename T2>
    requires(std::is_const_v<T2> && !std::is_const_v<T>)
  CroppedArray1DRef(CroppedArray1DRef<T2> RHS) = delete;

  // Can not change type.
  template <typename T2>
    requires(!(std::is_const_v<T2> && !std::is_const_v<T>) &&
             !std::is_same_v<std::remove_const_t<T>, std::remove_const_t<T2>>)
  CroppedArray1DRef(CroppedArray1DRef<T2> RHS) = delete;

  // Conversion from CroppedArray1DRef<T> to CroppedArray1DRef<const T>.
  template <typename T2>
    requires(!std::is_const_v<T2> && std::is_const_v<T> &&
             std::is_same_v<std::remove_const_t<T>, std::remove_const_t<T2>>)
  CroppedArray1DRef( // NOLINT(google-explicit-constructor)
      CroppedArray1DRef<T2> RHS)
      : CroppedArray1DRef(RHS.base, RHS.offset, RHS.numElts) {}

  [[nodiscard]] Array1DRef<T> getAsArray1DRef() const {
    return {begin(), size()};
  }

  [[nodiscard]] CroppedArray1DRef<T> getCrop(int offset, int numElts) const;
  [[nodiscard]] CroppedArray1DRef<T> getBlock(int size, int index) const;

  [[nodiscard]] T* begin() const;
  [[nodiscard]] T* end() const;

  [[nodiscard]] int RAWSPEED_READONLY size() const;

  [[nodiscard]] T* addressOf(int eltIdx) const;
  [[nodiscard]] T& operator()(int eltIdx) const;
};

// CTAD deduction guide
template <typename T>
CroppedArray1DRef(Array1DRef<T> base, int offset,
                  int numElts) -> CroppedArray1DRef<T>;

template <class T>
__attribute__((always_inline)) inline void
CroppedArray1DRef<T>::establishClassInvariants() const noexcept {
  base.establishClassInvariants();
  invariant(offset >= 0);
  invariant(numElts >= 0);
  invariant(offset <= base.size());
  invariant(numElts <= base.size());
  invariant(offset + numElts <= base.size());
}

template <class T>
inline CroppedArray1DRef<T>::CroppedArray1DRef(Array1DRef<T> base_,
                                               const int offset_,
                                               const int numElts_)
    : base(base_), offset(offset_), numElts(numElts_) {
  establishClassInvariants();
}

template <class T>
[[nodiscard]] inline CroppedArray1DRef<T>
CroppedArray1DRef<T>::getCrop(int additionalOffset, int size) const {
  establishClassInvariants();
  invariant(additionalOffset >= 0);
  invariant(size >= 0);
  invariant(additionalOffset <= numElts);
  invariant(size <= numElts);
  invariant(additionalOffset + size <= numElts);
  return base.getCrop(offset + additionalOffset, size);
}

template <class T>
[[nodiscard]] inline CroppedArray1DRef<T>
CroppedArray1DRef<T>::getBlock(int size, int index) const {
  establishClassInvariants();
  invariant(index >= 0);
  invariant(size >= 0);
  invariant(index <= numElts);
  invariant(size <= numElts);
  const int additionalOffset = size * index;
  invariant(additionalOffset <= numElts);
  invariant(additionalOffset + size <= numElts);
  return getCrop(additionalOffset, size);
}

template <class T> inline T* CroppedArray1DRef<T>::begin() const {
  establishClassInvariants();
  return addressOf(/*eltIdx=*/0);
}
template <class T> inline T* CroppedArray1DRef<T>::end() const {
  establishClassInvariants();
  return addressOf(/*eltIdx=*/numElts);
}

template <class T> inline int CroppedArray1DRef<T>::size() const {
  establishClassInvariants();
  return numElts;
}

template <class T>
inline T* CroppedArray1DRef<T>::addressOf(const int eltIdx) const {
  establishClassInvariants();
  invariant(eltIdx >= 0);
  invariant(eltIdx <= numElts);
  return base.addressOf(offset + eltIdx);
}

template <class T>
inline T& CroppedArray1DRef<T>::operator()(const int eltIdx) const {
  establishClassInvariants();
  invariant(eltIdx >= 0);
  invariant(eltIdx < numElts);
  return *addressOf(eltIdx);
}

} // namespace rawspeed
