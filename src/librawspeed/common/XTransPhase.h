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

#include "adt/Array2DRef.h"
#include "adt/Optional.h"
#include "adt/Point.h"
#include "metadata/ColorFilterArray.h"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdlib>

namespace rawspeed {

using XTransPhase = iPoint2D;

inline iPoint2D getTranslationalOffset(XTransPhase src, XTransPhase tgt) {
  iPoint2D off = tgt - src;
  return {std::abs(off.x), std::abs(off.y)};
}

// NOTE: phase shift is direction-independent (phase order does not matter.)
template <typename T>
inline std::array<T, 6 * 6> applyPhaseShift(std::array<T, 6 * 6> srcData,
                                            XTransPhase srcPhase,
                                            XTransPhase tgtPhase) {
  const iPoint2D coordOffset = getTranslationalOffset(srcPhase, tgtPhase);
  assert(coordOffset >= iPoint2D(0, 0) && "Offset is non-negative.");
  const Array2DRef<const T> src(srcData.data(), 6, 6);

  std::array<T, 6 * 6> tgtData;
  const Array2DRef<T> tgt(tgtData.data(), 6, 6);
  for (int row = 0; row < tgt.height(); ++row) {
    for (int col = 0; col < tgt.width(); ++col) {
      tgt(row, col) = src((coordOffset.y + row) % 6, (coordOffset.x + col) % 6);
    }
  }

  return tgtData;
}

inline std::array<CFAColor, 6 * 6> getAsCFAColors(XTransPhase p) {
  const XTransPhase basePhase(0, 0);
  // From Fujifilm X-Pro1.
  using enum CFAColor;
  const std::array<CFAColor, 6 * 6> basePat = {
      GREEN, GREEN, RED,  GREEN, GREEN, BLUE,  GREEN, GREEN, BLUE,
      GREEN, GREEN, RED,  BLUE,  RED,   GREEN, RED,   BLUE,  GREEN,
      GREEN, GREEN, BLUE, GREEN, GREEN, RED,   GREEN, GREEN, RED,
      GREEN, GREEN, BLUE, RED,   BLUE,  GREEN, BLUE,  RED,   GREEN};
  return applyPhaseShift(basePat, basePhase, /*tgtPhase=*/p);
}

inline Optional<XTransPhase> getAsXTransPhase(const ColorFilterArray& CFA) {
  if (CFA.getSize() != iPoint2D(6, 6))
    return {};

  std::array<CFAColor, 6 * 6> patData;
  const Array2DRef<CFAColor> pat(patData.data(), 6, 6);
  for (int row = 0; row < pat.height(); ++row) {
    for (int col = 0; col < pat.width(); ++col) {
      pat(row, col) = CFA.getColorAt(col, row);
    }
  }

  iPoint2D off;
  for (off.y = 0; off.y < pat.height(); ++off.y) {
    for (off.x = 0; off.x < pat.width(); ++off.x) {
      if (getAsCFAColors(off) == patData)
        return off;
    }
  }

  return {};
}

} // namespace rawspeed
