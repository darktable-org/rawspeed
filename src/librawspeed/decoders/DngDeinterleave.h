/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2026 Mayk Thewessen

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

#include <cassert>
#include <cstdint>
#include <vector>

namespace rawspeed {

// Pixel de-interleave for DNG 1.7 RowInterleaveFactor (0xC71F) /
// ColumnInterleaveFactor (0xCD43).
//
// A CFA DNG may store its mosaic as `R` x `C` stacked "fields" (color-plane
// subimages), each of which compresses far better under lossy JPEG XL than the
// raw mosaic. De-interleaving REORDERS pixels only; it never resizes, so the
// final dimensions equal the stored (decoded) dimensions.
//
// Field f's rows scatter to final rows f, f+R, f+2R, ... i.e. the forward map
// stored->final is `fy = within_field_row * R + field_row_index`, and
// symmetrically for columns with C. Non-divisible sizes are handled exactly
// like the dng_sdk reference (dng_read_image.cpp): earlier fields absorb the
// remainder.
//
// This matches the Adobe DNG 1.7.1.0 specification and the dng_sdk reference
// implementation.

// Build the stored-row -> final-row (or stored-col -> final-col) lookup table.
//
// `total` is the stored extent (height for rows, width for columns) and
// `factor` is the corresponding interleave factor (R or C, >= 1).
//
// Returns a vector `map` of size `total` such that the pixel stored at index
// `s` belongs at final index `map[s]`.
[[nodiscard]] inline std::vector<int> dngDeinterleaveFieldMap(int total,
                                                              int factor) {
  assert(total >= 0);
  assert(factor >= 1);

  std::vector<int> map(static_cast<size_t>(total));

  int acc = 0;
  for (int f = 0; f < factor; ++f) {
    // Number of rows/cols in this field. Earlier fields absorb the remainder,
    // matching dng_sdk: rows[f] = (total - f + factor - 1) / factor.
    const int fieldExtent = (total - f + factor - 1) / factor;
    for (int within = 0; within < fieldExtent; ++within) {
      const int storedIdx = acc + within;
      assert(storedIdx < total);
      map[static_cast<size_t>(storedIdx)] = within * factor + f;
    }
    acc += fieldExtent;
  }
  assert(acc == total);

  return map;
}

} // namespace rawspeed
