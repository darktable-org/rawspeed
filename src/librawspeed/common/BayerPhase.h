/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2022 Roman Lebedev

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

#include "common/Array2DRef.h"         // for Array2DRef
#include "common/Point.h"              // for iPoint2D
#include "metadata/ColorFilterArray.h" // for CFAColor, CFAColor::GREEN
#include <algorithm>                   // for find_if, equal
#include <array>                       // for array, operator==
#include <cassert>                     // for assert
#include <cmath>                       // for abs
#include <cstdlib>                     // for abs
#include <iterator>                    // for distance
#include <optional>                    // for optional
#include <utility>                     // for swap

namespace rawspeed {

// Bayer CFA 2x2 pattern has only 3 distinct colors - red, green (x2) and blue,
// and greens are always on diagonal, thus the actual pattern always looks like:
//  ..........
//  ..RGRGRG..
//  ..GBGBGB..
//  ..RGRGRG..
//  ..GBGBGB..
//  ..RGRGRG..
//  ..GBGBGB..
//  ..........
// and there are only 4 flavours of the 2x2 pattern, since position is mod-2:
enum class BayerPhase : int {
  // The top-left pixel of the image is red pixel.
  RGGB = 0b00, // 0
  // The top-left pixel of the image is green pixel in a green/red row.
  GRBG = 0b01, // 1
  // The top-left pixel of the image is green pixel in a green/blue row.
  GBRG = 0b10, // 2
  // he top-left pixel of the image is blue pixel.
  BGGR = 0b11, // 3

  // COLUMN_SHIFT = 0b01,
  // ROW_SHIFT    = 0b10
};

// R  G0 R  G0
// G1 B  G1 B
// R  G0 R  G0
// G1 B  G1 B
inline iPoint2D getTranslationalOffset(BayerPhase src, BayerPhase tgt) {
  auto getCanonicalPosition = [](BayerPhase p) -> iPoint2D {
    auto i = static_cast<unsigned>(p);
    return {(i & 0b01) != 0, (i & 0b10) != 0};
  };

  iPoint2D off = getCanonicalPosition(tgt) - getCanonicalPosition(src);
  return {std::abs(off.x), std::abs(off.y)};
}

// NOTE: phase shift is direction-independent (phase order does not matter.)
template <typename T>
inline std::array<T, 4> applyPhaseShift(std::array<T, 4> srcData,
                                        BayerPhase srcPhase,
                                        BayerPhase tgtPhase) {
  const iPoint2D coordOffset = getTranslationalOffset(srcPhase, tgtPhase);
  assert(coordOffset >= iPoint2D(0, 0) && "Offset is non-negative.");
  const Array2DRef<const T> src(srcData.data(), 2, 2);

  std::array<T, 4> tgtData;
  const Array2DRef<T> tgt(tgtData.data(), 2, 2);
  for (int row = 0; row < tgt.height; ++row) {
    for (int col = 0; col < tgt.width; ++col) {
      tgt(row, col) = src((coordOffset.y + row) % 2, (coordOffset.x + col) % 2);
    }
  }

  return tgtData;
}

inline std::array<CFAColor, 4> getAsCFAColors(BayerPhase p) {
  const BayerPhase basePhase = BayerPhase::RGGB;
  const std::array<CFAColor, 4> basePat = {CFAColor::RED, CFAColor::GREEN,
                                           CFAColor::GREEN, CFAColor::BLUE};
  return applyPhaseShift(basePat, basePhase, /*tgtPhase=*/p);
}

// Remap data between these two Bayer phases,
// while preserving relative order of 'green' values.
template <typename T>
inline std::array<T, 4> applyStablePhaseShift(std::array<T, 4> srcData,
                                              BayerPhase srcPhase,
                                              BayerPhase tgtPhase) {
  std::array<T, 4> tgtData = applyPhaseShift(srcData, srcPhase, tgtPhase);

  if (!/*rowsSwapped=*/getTranslationalOffset(srcPhase, tgtPhase).y)
    return tgtData;

  auto is_green = [](const CFAColor& c) { return c == CFAColor::GREEN; };

  const std::array<CFAColor, 4> tgtColors = getAsCFAColors(tgtPhase);
  int green0Idx =
      std::distance(tgtColors.begin(),
                    std::find_if(tgtColors.begin(), tgtColors.end(), is_green));
  int green1Idx = std::distance(std::find_if(tgtColors.rbegin(),
                                             tgtColors.rend(), is_green),
                                tgtColors.rend()) -
                  1;

  std::swap(tgtData[green0Idx], tgtData[green1Idx]);

  return tgtData;
}

inline std::optional<BayerPhase> getAsBayerPhase(const ColorFilterArray& CFA) {
  if (CFA.getSize() != iPoint2D(2, 2))
    return {};

  std::array<CFAColor, 4> patData;
  const Array2DRef<CFAColor> pat(patData.data(), 2, 2);
  for (int row = 0; row < pat.height; ++row) {
    for (int col = 0; col < pat.width; ++col) {
      pat(row, col) = CFA.getColorAt(col, row);
    }
  }

  for (auto i = (int)BayerPhase::RGGB; i <= (int)BayerPhase::BGGR; ++i) {
    if (auto p = static_cast<BayerPhase>(i); getAsCFAColors(p) == patData)
      return p;
  }

  return {};
}

} // namespace rawspeed
