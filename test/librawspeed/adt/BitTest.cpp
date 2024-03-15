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

#include "adt/Bit.h"
#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <tuple>
#include <vector>
#include <gtest/gtest.h>

using rawspeed::clampBits;
using rawspeed::isPowerOfTwo;
using std::make_tuple;
using std::min;
using std::numeric_limits;
using std::string;
using std::vector;

namespace rawspeed_test {

using powerOfTwoType = std::tuple<int, bool>;
class PowerOfTwoTest : public ::testing::TestWithParam<powerOfTwoType> {
protected:
  PowerOfTwoTest() = default;
  virtual void SetUp() {
    in = std::get<0>(GetParam());
    expected = std::get<1>(GetParam());
  }

  int in;        // input
  bool expected; // expected output
};
static const powerOfTwoType powerOfTwoValues[] = {
    make_tuple(0, true),  make_tuple(1, true),   make_tuple(2, true),
    make_tuple(3, false), make_tuple(4, true),   make_tuple(5, false),
    make_tuple(6, false), make_tuple(7, false),  make_tuple(8, true),
    make_tuple(9, false), make_tuple(10, false), make_tuple(11, false),

};
INSTANTIATE_TEST_SUITE_P(PowerOfTwoTest, PowerOfTwoTest,
                         ::testing::ValuesIn(powerOfTwoValues));
TEST_P(PowerOfTwoTest, PowerOfTwoTest) {
  ASSERT_EQ(isPowerOfTwo(in), expected);
}

using ClampBitsType = std::tuple<int, int, uint16_t>;
class ClampBitsTest : public ::testing::TestWithParam<ClampBitsType> {
protected:
  ClampBitsTest() = default;
  virtual void SetUp() {
    in = std::get<0>(GetParam());
    n = std::get<1>(GetParam());
    expected = std::get<2>(GetParam());
  }

  int in; // input
  int n;
  uint16_t expected; // expected output
};

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define ROW(v, p, pv) make_tuple((v), (p), MIN(pv, MAX(v, 0))),

#define ROWS(v, p, pv) ROW(-v, p, 0) ROW(v, p, pv)

#define THREEROWS(v, p)                                                        \
  ROWS(((1 << (v)) - 1), (p), ((1 << (p)) - 1))                                \
  ROWS(((1 << (v)) - 0), (p), ((1 << (p)) - 1))                                \
  ROWS(((1 << (v)) + 1), (p), ((1 << (p)) - 1))

#define MOREROWS(v)                                                            \
  THREEROWS(v, 0)                                                              \
  THREEROWS(v, 1)                                                              \
  THREEROWS(v, 2)                                                              \
  THREEROWS(v, 4)                                                              \
  THREEROWS(v, 8) THREEROWS(v, 16)

#define GENERATE()                                                             \
  MOREROWS(0)                                                                  \
  MOREROWS(1)                                                                  \
  MOREROWS(2) MOREROWS(4) MOREROWS(8) MOREROWS(16) MOREROWS(24) MOREROWS(30)

static const ClampBitsType ClampBitsValues[] = {
    make_tuple(0, 0, 0),    make_tuple(0, 16, 0),
    make_tuple(32, 0, 0),   make_tuple(32, 16, 32),
    make_tuple(32, 2, 3),   make_tuple(-32, 0, 0),
    make_tuple(-32, 16, 0), GENERATE()};
INSTANTIATE_TEST_SUITE_P(ClampBitsTest, ClampBitsTest,
                         ::testing::ValuesIn(ClampBitsValues));
TEST_P(ClampBitsTest, ClampBitsTest) { ASSERT_EQ(clampBits(in, n), expected); }
TEST(ClampBitsDeathTest, Only16Bit) {
#ifndef NDEBUG
  ASSERT_DEATH({ ASSERT_EQ(clampBits(0, 17), 0); }, "nBits <= 16");
#endif
}

TEST(ClampBitsUnsignedDeathTest, NoNopClamps) {
#ifndef NDEBUG
  ASSERT_DEATH(
      { ASSERT_EQ(clampBits<uint16_t>(0, 16), 0); },
      "bitwidth<T>\\(\\) > nBits");
#endif
}

} // namespace rawspeed_test
