/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2017 Roman Lebedev

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; withexpected even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include "common/Range.h"     // for Range
#include "common/RangeTest.h" // for RangeDoesntContain, RangeContains
#include <gtest/gtest.h>      // for TEST, ASSERT_EQ, ASSERT_TRUE, ASSERT_FALSE
#include <set>                // for set

using rawspeed::Range;

namespace rawspeed_test {

// check the basic - begin/end works

TEST(RangeTest, BasicSignedRangeForTest) {
  const Range<int> r0(0, 3U);
  ASSERT_EQ(r0.begin(), 0);
  ASSERT_EQ(r0.end(), 3);

  const Range<int> r1(-1, 3U);
  ASSERT_EQ(r1.begin(), -1);
  ASSERT_EQ(r1.end(), 2);
}

// check that RangeContains() works properly

TEST(RangeTest, BasicSignedContainsTest) {
  const Range<int> r0(0, 3U);
  ASSERT_PRED_FORMAT2(RangeContains, r0, r0.begin());

  ASSERT_PRED_FORMAT2(RangeDoesntContain, r0, -4);
  ASSERT_PRED_FORMAT2(RangeDoesntContain, r0, -3);
  ASSERT_PRED_FORMAT2(RangeDoesntContain, r0, -2);
  ASSERT_PRED_FORMAT2(RangeDoesntContain, r0, -1);
  ASSERT_PRED_FORMAT2(RangeContains, r0, 0);
  ASSERT_PRED_FORMAT2(RangeContains, r0, 1);
  ASSERT_PRED_FORMAT2(RangeContains, r0, 2);
  ASSERT_PRED_FORMAT2(RangeDoesntContain, r0, 3);
  ASSERT_PRED_FORMAT2(RangeDoesntContain, r0, 4);
  ASSERT_PRED_FORMAT2(RangeDoesntContain, r0, 5);
  ASSERT_PRED_FORMAT2(RangeDoesntContain, r0, 6);
}

TEST(RangeTest, BasicSignedCrossoverContainsTest) {
  const Range<int> r0(-1, 3U);
  ASSERT_PRED_FORMAT2(RangeContains, r0, r0.begin());

  ASSERT_PRED_FORMAT2(RangeDoesntContain, r0, -5);
  ASSERT_PRED_FORMAT2(RangeDoesntContain, r0, -4);
  ASSERT_PRED_FORMAT2(RangeDoesntContain, r0, -3);
  ASSERT_PRED_FORMAT2(RangeDoesntContain, r0, -2);
  ASSERT_PRED_FORMAT2(RangeContains, r0, -1);
  ASSERT_PRED_FORMAT2(RangeContains, r0, 0);
  ASSERT_PRED_FORMAT2(RangeContains, r0, 1);
  ASSERT_PRED_FORMAT2(RangeDoesntContain, r0, 2);
  ASSERT_PRED_FORMAT2(RangeDoesntContain, r0, 3);
  ASSERT_PRED_FORMAT2(RangeDoesntContain, r0, 4);
  ASSERT_PRED_FORMAT2(RangeDoesntContain, r0, 5);
}

TEST(RangeTest, BasicUnsignedContainsTest) {
  const Range<unsigned> r0(4, 3U);
  ASSERT_PRED_FORMAT2(RangeContains, r0, r0.begin());

  ASSERT_PRED_FORMAT2(RangeDoesntContain, r0, 0U);
  ASSERT_PRED_FORMAT2(RangeDoesntContain, r0, 1U);
  ASSERT_PRED_FORMAT2(RangeDoesntContain, r0, 2U);
  ASSERT_PRED_FORMAT2(RangeDoesntContain, r0, 3U);
  ASSERT_PRED_FORMAT2(RangeContains, r0, 4U);
  ASSERT_PRED_FORMAT2(RangeContains, r0, 5U);
  ASSERT_PRED_FORMAT2(RangeContains, r0, 6U);
  ASSERT_PRED_FORMAT2(RangeDoesntContain, r0, 7U);
  ASSERT_PRED_FORMAT2(RangeDoesntContain, r0, 8U);
  ASSERT_PRED_FORMAT2(RangeDoesntContain, r0, 9U);
  ASSERT_PRED_FORMAT2(RangeDoesntContain, r0, 10U);
}

TEST(RangeTest, SignedZeroSizeContainsTest) {
  const Range<int> r0(0, 0U);
  ASSERT_PRED_FORMAT2(RangeDoesntContain, r0, r0.begin());

  ASSERT_PRED_FORMAT2(RangeDoesntContain, r0, -2);
  ASSERT_PRED_FORMAT2(RangeDoesntContain, r0, -1);
  ASSERT_PRED_FORMAT2(RangeDoesntContain, r0, 0);
  ASSERT_PRED_FORMAT2(RangeDoesntContain, r0, 1);
  ASSERT_PRED_FORMAT2(RangeDoesntContain, r0, 2);
}

TEST(RangeTest, UnsignedZeroSizeContainsTest) {
  const Range<unsigned> r0(1, 0U);
  ASSERT_PRED_FORMAT2(RangeDoesntContain, r0, r0.begin());

  ASSERT_PRED_FORMAT2(RangeDoesntContain, r0, 0U);
  ASSERT_PRED_FORMAT2(RangeDoesntContain, r0, 1U);
  ASSERT_PRED_FORMAT2(RangeDoesntContain, r0, 2U);
  ASSERT_PRED_FORMAT2(RangeDoesntContain, r0, 3U);
  ASSERT_PRED_FORMAT2(RangeDoesntContain, r0, 4U);
}

// now let's test overlap. the test is symmetrical.

TEST_P(TwoRangesTest, OverlapDataTest) {
  if (AllOverlapped.find(GetParam()) != AllOverlapped.end()) {
    ASSERT_PRED_FORMAT2(RangesOverlap, r0, r1);
  } else {
    ASSERT_PRED_FORMAT2(RangesDontOverlap, r0, r1);
  }
}

} // namespace rawspeed_test
