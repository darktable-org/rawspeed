/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2021 Roman Lebedev

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
#include "adt/Array2DRef.h"
#include "adt/CroppedArray1DRef.h"
#include "adt/Invariant.h"
#include <type_traits>

namespace rawspeed {

template <class T> class CroppedArray2DRef final {
  const Array2DRef<T> base;

  void establishClassInvariants() const noexcept;

  // We need to be able to convert to const version.
  friend CroppedArray2DRef<const T>;

public:
  using value_type = T;
  using cvless_value_type = std::remove_cv_t<value_type>;

  int offsetCols = 0;
  int offsetRows = 0;
  int croppedWidth = 0;
  int croppedHeight = 0;

  CroppedArray2DRef() = default;

  // Can not cast away constness.
  template <typename T2>
    requires(std::is_const_v<T2> && !std::is_const_v<T>)
  CroppedArray2DRef(CroppedArray2DRef<T2> RHS) = delete;

  // Can not change type.
  template <typename T2>
    requires(!(std::is_const_v<T2> && !std::is_const_v<T>) &&
             !std::is_same_v<std::remove_const_t<T>, std::remove_const_t<T2>>)
  CroppedArray2DRef(CroppedArray2DRef<T2> RHS) = delete;

  // Conversion from Array2DRef<T> to CroppedArray2DRef<T>.
  CroppedArray2DRef(Array2DRef<T> RHS) // NOLINT google-explicit-constructor
      : base(RHS), croppedWidth(base.width), croppedHeight(base.height) {}

  CroppedArray2DRef(Array2DRef<T> base_, int offsetCols_, int offsetRows_,
                    int croppedWidth_, int croppedHeight_);

  // Conversion from CroppedArray2DRef<T> to CroppedArray2DRef<const T>.
  template <class T2>
    requires(!std::is_const_v<T2> && std::is_const_v<T> &&
             std::is_same_v<std::remove_const_t<T>, std::remove_const_t<T2>>)
  CroppedArray2DRef( // NOLINT google-explicit-constructor
      CroppedArray2DRef<T2> RHS)
      : base(RHS.base), offsetCols(RHS.offsetCols), offsetRows(RHS.offsetRows),
        croppedWidth(RHS.croppedWidth), croppedHeight(RHS.croppedHeight) {}

  CroppedArray1DRef<T> operator[](int row) const;

  T& operator()(int row, int col) const;
};

// CTAD deduction guide
template <typename T>
explicit CroppedArray2DRef(Array2DRef<T> base_, int offsetCols_,
                           int offsetRows_, int croppedWidth_,
                           int croppedHeight_)
    -> CroppedArray2DRef<typename Array2DRef<T>::value_type>;

template <class T>
inline void CroppedArray2DRef<T>::establishClassInvariants() const noexcept {
  invariant(offsetCols >= 0);
  invariant(offsetRows >= 0);
  invariant(croppedWidth >= 0);
  invariant(croppedHeight >= 0);
  invariant(offsetCols <= base.width);
  invariant(offsetRows <= base.height);
  invariant(croppedWidth <= base.width);
  invariant(croppedHeight <= base.height);
  invariant(offsetCols + croppedWidth <= base.width);
  invariant(offsetRows + croppedHeight <= base.height);
}

template <class T>
CroppedArray2DRef<T>::CroppedArray2DRef(Array2DRef<T> base_, int offsetCols_,
                                        int offsetRows_, int croppedWidth_,
                                        int croppedHeight_)
    : base(base_), offsetCols(offsetCols_), offsetRows(offsetRows_),
      croppedWidth(croppedWidth_), croppedHeight(croppedHeight_) {
  establishClassInvariants();
}

template <class T>
inline CroppedArray1DRef<T>
CroppedArray2DRef<T>::operator[](const int row) const {
  establishClassInvariants();
  invariant(row >= 0);
  invariant(row < croppedHeight);
  const Array1DRef<T> fullLine = base.operator[](offsetRows + row);
  return fullLine.getCrop(offsetCols, croppedWidth);
}

template <class T>
inline T& CroppedArray2DRef<T>::operator()(const int row, const int col) const {
  establishClassInvariants();
  invariant(col >= 0);
  invariant(col < croppedWidth);
  return (operator[](row))(col);
}

} // namespace rawspeed
