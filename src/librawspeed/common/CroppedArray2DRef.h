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

#include "common/Array2DRef.h"
#include <cassert> // for assert
#include <type_traits>

namespace rawspeed {

template <class T> class CroppedArray2DRef {
  const Array2DRef<T> base;

  // We need to be able to convert to const version.
  friend CroppedArray2DRef<const T>;

  inline T& operator[](int row) const;

public:
  using value_type = T;
  using cvless_value_type = typename std::remove_cv<value_type>::type;

  int offsetCols = 0, offsetRows = 0;
  int croppedWidth = 0, croppedHeight = 0;

  CroppedArray2DRef() = default;

  CroppedArray2DRef(Array2DRef<T> base_, int offsetCols_, int offsetRows_,
                    int croppedWidth_, int croppedHeight_);

  // Conversion from CroppedArray2DRef<T> to CroppedArray2DRef<const T>.
  template <class T2, typename = std::enable_if_t<std::is_same<
                          typename std::remove_const<T>::type, T2>::value>>
  CroppedArray2DRef( // NOLINT google-explicit-constructor
      CroppedArray2DRef<T2> RHS)
      : base(RHS.base), offsetCols(RHS.offsetCols), offsetRows(RHS.offsetRows),
        croppedWidth(RHS.croppedWidth), croppedHeight(RHS.croppedHeight) {}

  inline T& operator()(int row, int col) const;
};

template <class T>
CroppedArray2DRef<T>::CroppedArray2DRef(Array2DRef<T> base_, int offsetCols_,
                                        int offsetRows_, int croppedWidth_,
                                        int croppedHeight_)
    : base(base_), offsetCols(offsetCols_), offsetRows(offsetRows_),
      croppedWidth(croppedWidth_), croppedHeight(croppedHeight_) {
  assert(offsetCols_ >= 0);
  assert(offsetRows_ >= 0);
  assert(croppedWidth_ >= 0);
  assert(croppedHeight_ >= 0);
  assert(offsetCols_ + croppedWidth_ <= base.width);
  assert(offsetRows_ + croppedHeight_ <= base.height);
}

template <class T> T& CroppedArray2DRef<T>::operator[](const int row) const {
  assert(row >= 0);
  assert(row < croppedHeight);
  return base.operator()(offsetRows + row, /*col=*/0);
}

template <class T>
T& CroppedArray2DRef<T>::operator()(const int row, const int col) const {
  assert(col >= 0);
  assert(col < croppedWidth);
  return (&(operator[](row)))[offsetCols + col];
}

} // namespace rawspeed
