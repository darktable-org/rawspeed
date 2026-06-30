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

#include "decoders/DngDeinterleave.h"
#include <vector>
#include <gtest/gtest.h>

using rawspeed::dngDeinterleaveFieldMap;

namespace rawspeed_test {

namespace {

// Apply the row/col field maps to a stored (row-major, single-channel) image,
// mirroring DngDecoder::deinterleaveFields for verification of the index math.
std::vector<int> applyDeinterleave(const std::vector<int>& stored, int storedH,
                                   int storedW, int rowFactor, int colFactor) {
  const std::vector<int> rowMap =
      dngDeinterleaveFieldMap(storedH, rowFactor);
  const std::vector<int> colMap =
      dngDeinterleaveFieldMap(storedW, colFactor);

  std::vector<int> out(stored.size());
  for (int sy = 0; sy < storedH; ++sy) {
    const int fy = rowMap[sy];
    for (int sx = 0; sx < storedW; ++sx) {
      const int fx = colMap[sx];
      out[fy * storedW + fx] = stored[sy * storedW + sx];
    }
  }
  return out;
}

} // namespace

// Identity: factor 1 must leave the map untouched.
TEST(DngDeinterleaveTest, FactorOneIsIdentity) {
  const std::vector<int> map = dngDeinterleaveFieldMap(7, 1);
  ASSERT_EQ(map.size(), 7U);
  for (int i = 0; i < 7; ++i)
    EXPECT_EQ(map[i], i);
}

// The exact stored->final row map for R=2 on 4 rows.
// Field 0 occupies stored rows {0,1} -> final {0,2}.
// Field 1 occupies stored rows {2,3} -> final {1,3}.
TEST(DngDeinterleaveTest, RowMap2x4) {
  const std::vector<int> map = dngDeinterleaveFieldMap(4, 2);
  const std::vector<int> expected = {0, 2, 1, 3};
  EXPECT_EQ(map, expected);
}

// Non-divisible case: earlier fields absorb the remainder (dng_sdk rule).
// total=5, factor=2: field0 has rows {0,1,2}, field1 has rows {3,4}.
//   stored 0 -> 0, 1 -> 2, 2 -> 4 (field 0, *2+0)
//   stored 3 -> 1, 4 -> 3       (field 1, *2+1)
TEST(DngDeinterleaveTest, NonDivisibleRemainder) {
  const std::vector<int> map = dngDeinterleaveFieldMap(5, 2);
  const std::vector<int> expected = {0, 2, 4, 1, 3};
  EXPECT_EQ(map, expected);
}

// total=7, factor=3: field0 {0,1,2}(3), field1 {3,4}(2), field2 {5,6}(2).
//   field0: 0->0, 1->3, 2->6
//   field1: 3->1, 4->4
//   field2: 5->2, 6->5
TEST(DngDeinterleaveTest, NonDivisibleThreeFields) {
  const std::vector<int> map = dngDeinterleaveFieldMap(7, 3);
  const std::vector<int> expected = {0, 3, 6, 1, 4, 2, 5};
  EXPECT_EQ(map, expected);
}

// The full VERIFIED 4x4, R=C=2 worked example.
// A stored buffer whose 4 quadrants are constant (TL=10 "R", TR=20 "G",
// BL=30 "G", BR=40 "B") must de-interleave to the RGGB-phase mosaic:
//   10 20 10 20 / 30 40 30 40 / 10 20 10 20 / 30 40 30 40
TEST(DngDeinterleaveTest, WorkedExample4x4Quadrants) {
  // clang-format off
  const std::vector<int> stored = {
      10, 10, 20, 20,
      10, 10, 20, 20,
      30, 30, 40, 40,
      30, 30, 40, 40,
  };
  const std::vector<int> expected = {
      10, 20, 10, 20,
      30, 40, 30, 40,
      10, 20, 10, 20,
      30, 40, 30, 40,
  };
  // clang-format on

  const std::vector<int> out =
      applyDeinterleave(stored, /*storedH=*/4, /*storedW=*/4,
                        /*rowFactor=*/2, /*colFactor=*/2);
  EXPECT_EQ(out, expected);
}

// Per-pixel stored->final mapping from the spec, every pixel of the 4x4.
TEST(DngDeinterleaveTest, WorkedExample4x4PerPixelMap) {
  const std::vector<int> rowMap = dngDeinterleaveFieldMap(4, 2);
  const std::vector<int> colMap = dngDeinterleaveFieldMap(4, 2);

  // (stored_y, stored_x) -> (final_y, final_x)
  const int expected[4][4][2] = {
      {{0, 0}, {0, 2}, {0, 1}, {0, 3}},
      {{2, 0}, {2, 2}, {2, 1}, {2, 3}},
      {{1, 0}, {1, 2}, {1, 1}, {1, 3}},
      {{3, 0}, {3, 2}, {3, 1}, {3, 3}},
  };
  for (int sy = 0; sy < 4; ++sy) {
    for (int sx = 0; sx < 4; ++sx) {
      EXPECT_EQ(rowMap[sy], expected[sy][sx][0]) << "sy=" << sy << " sx=" << sx;
      EXPECT_EQ(colMap[sx], expected[sy][sx][1]) << "sy=" << sy << " sx=" << sx;
    }
  }
}

} // namespace rawspeed_test
