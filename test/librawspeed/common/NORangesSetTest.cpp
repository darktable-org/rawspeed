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

#include "common/NORangesSet.h" // for NORangesSet
#include "common/Range.h"       // for Range
#include "common/RangeTest.h"   // for RangeDoesntContain, RangeContains
#include <gtest/gtest.h>        // for TEST, ASSERT_EQ, ASSERT_TRUE, ASSERT_F...
#include <utility>              // for pair
// IWYU pragma: no_forward_declare rawspeed::Range

using rawspeed::NORangesSet;
using rawspeed::Range;

namespace rawspeed_test {

TEST_P(TwoRangesTest, NORangesSetDataSelfTest) {
  {
    NORangesSet<Range<int>> s;

    auto res = s.emplace(r0);
    ASSERT_TRUE(res.second);

    // can not insert same element twice
    res = s.emplace(r0);
    ASSERT_FALSE(res.second);
  }
  {
    NORangesSet<Range<int>> s;

    auto res = s.emplace(r1);
    ASSERT_TRUE(res.second);

    // can not insert same element twice
    res = s.emplace(r1);
    ASSERT_FALSE(res.second);
  }
}

TEST_P(TwoRangesTest, NORangesSetDataTest) {
  {
    NORangesSet<Range<int>> s;
    auto res = s.emplace(r0);
    ASSERT_TRUE(res.second);

    res = s.emplace(r1);
    // if the ranges overlap, we should fail to insert the second range
    if (AllOverlapped.find(GetParam()) != AllOverlapped.end()) {
      ASSERT_FALSE(res.second);
    } else {
      ASSERT_TRUE(res.second);
    }
  }
  {
    NORangesSet<Range<int>> s;
    auto res = s.emplace(r1);
    ASSERT_TRUE(res.second);

    res = s.emplace(r0);
    // if the ranges overlap, we should fail to insert the second range
    if (AllOverlapped.find(GetParam()) != AllOverlapped.end()) {
      ASSERT_FALSE(res.second);
    } else {
      ASSERT_TRUE(res.second);
    }
  }
}

} // namespace rawspeed_test
