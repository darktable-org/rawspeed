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

#include "common/NORangesSet.h" // for NORangesSet, operator!=, set, set<>:...
#include "common/Range.h"       // for Range, RangesOverlap
#include "common/RangeTest.h"   // for AllOverlapped, TwoRangesTest, twoRan...
#include <gtest/gtest.h>        // for ParamIteratorInterface, AssertionResult
#include <string>               // for string, allocator
#include <tuple>                // for get, tuple
#include <utility>              // for __tuple_element_t
// IWYU pragma: no_forward_declare rawspeed::Range

using rawspeed::NORangesSet;
using rawspeed::Range;

namespace rawspeed_test {

TEST_P(TwoRangesTest, NORangesSetDataSelfTest) {
  {
    NORangesSet<Range<int>> s;

    auto res = s.insert(r0);
    ASSERT_TRUE(res);

    // can not insert same element twice
    res = s.insert(r0);
    ASSERT_FALSE(res);
  }
  {
    NORangesSet<Range<int>> s;

    auto res = s.insert(r1);
    ASSERT_TRUE(res);

    // can not insert same element twice
    res = s.insert(r1);
    ASSERT_FALSE(res);
  }
}

TEST_P(TwoRangesTest, NORangesSetDataTest) {
  {
    NORangesSet<Range<int>> s;
    auto res = s.insert(r0);
    ASSERT_TRUE(res);

    res = s.insert(r1);
    // if the ranges overlap, we should fail to insert the second range
    if (AllOverlapped.find(GetParam()) != AllOverlapped.end()) {
      ASSERT_FALSE(res);
    } else {
      ASSERT_TRUE(res);
    }
  }
  {
    NORangesSet<Range<int>> s;
    auto res = s.insert(r1);
    ASSERT_TRUE(res);

    res = s.insert(r0);
    // if the ranges overlap, we should fail to insert the second range
    if (AllOverlapped.find(GetParam()) != AllOverlapped.end()) {
      ASSERT_FALSE(res);
    } else {
      ASSERT_TRUE(res);
    }
  }
}

using threeRangesType = std::tuple<int, unsigned, int, unsigned, int, unsigned>;
class ThreeRangesTest : public ::testing::TestWithParam<threeRangesType> {
protected:
  ThreeRangesTest() = default;
  virtual void SetUp() {
    r0 = Range<int>(std::get<0>(GetParam()), std::get<1>(GetParam()));
    r1 = Range<int>(std::get<2>(GetParam()), std::get<3>(GetParam()));
    r2 = Range<int>(std::get<4>(GetParam()), std::get<5>(GetParam()));
  }

  Range<int> r0;
  Range<int> r1;
  Range<int> r2;
};
INSTANTIATE_TEST_CASE_P(
    Unsigned, ThreeRangesTest,
    testing::Combine(testing::Range(0, 3), testing::Range(0U, 3U),
                     testing::Range(0, 3), testing::Range(0U, 3U),
                     testing::Range(0, 3), testing::Range(0U, 3U)));

TEST_P(ThreeRangesTest, NORangesSetDataTest) {
  NORangesSet<Range<int>> s;
  auto res = s.insert(r0);
  ASSERT_TRUE(res);

  res = s.insert(r1);
  ASSERT_EQ(res, !RangesOverlap(r1, r0));
  if (!res)
    return; // If we already have overlap don't proceed further.

  res = s.insert(r2);
  ASSERT_EQ(res, !RangesOverlap(r0, r2) && !RangesOverlap(r1, r2));
}

} // namespace rawspeed_test
