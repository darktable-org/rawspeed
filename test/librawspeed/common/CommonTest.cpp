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

#include "common/Common.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <vector>
#include <gtest/gtest.h>

using rawspeed::copyPixels;
using rawspeed::isAligned;
using rawspeed::isIn;
using rawspeed::isPowerOfTwo;
using rawspeed::roundDown;
using rawspeed::roundUp;
using rawspeed::roundUpDivisionSafe;
using rawspeed::splitString;
using rawspeed::trimSpaces;
using std::make_tuple;
using std::min;
using std::numeric_limits;
using std::string;
using std::vector;

namespace rawspeed_test {

using RoundDownType = std::tuple<uint64_t, uint64_t, uint64_t>;
class RoundDownTest : public ::testing::TestWithParam<RoundDownType> {
protected:
  RoundDownTest() = default;
  virtual void SetUp() {
    in = std::get<0>(GetParam());
    multiple = std::get<1>(GetParam());
    expected = std::get<2>(GetParam());
  }

  uint64_t in; // input
  uint64_t multiple;
  uint64_t expected; // expected output
};
static const RoundDownType RoundDownValues[] = {
    make_tuple(0, 0, 0),    make_tuple(0, 10, 0),  make_tuple(10, 0, 10),
    make_tuple(10, 10, 10), make_tuple(10, 1, 10), make_tuple(10, 2, 10),
    make_tuple(10, 3, 9),   make_tuple(10, 4, 8),  make_tuple(10, 5, 10),
    make_tuple(10, 6, 6),   make_tuple(10, 7, 7),  make_tuple(10, 8, 8),
    make_tuple(10, 9, 9),   make_tuple(10, 11, 0), make_tuple(10, 12, 0),

};
INSTANTIATE_TEST_SUITE_P(RoundDownTest, RoundDownTest,
                         ::testing::ValuesIn(RoundDownValues));
TEST_P(RoundDownTest, RoundDownTest) {
  ASSERT_EQ(roundDown(in, multiple), expected);
}

using RoundUpType = std::tuple<uint64_t, uint64_t, uint64_t>;
class RoundUpTest : public ::testing::TestWithParam<RoundUpType> {
protected:
  RoundUpTest() = default;
  virtual void SetUp() {
    in = std::get<0>(GetParam());
    multiple = std::get<1>(GetParam());
    expected = std::get<2>(GetParam());
  }

  uint64_t in; // input
  uint64_t multiple;
  uint64_t expected; // expected output
};
static const RoundUpType RoundUpValues[] = {
    make_tuple(0, 0, 0),    make_tuple(0, 10, 0),   make_tuple(10, 0, 10),
    make_tuple(10, 10, 10), make_tuple(10, 1, 10),  make_tuple(10, 2, 10),
    make_tuple(10, 3, 12),  make_tuple(10, 4, 12),  make_tuple(10, 5, 10),
    make_tuple(10, 6, 12),  make_tuple(10, 7, 14),  make_tuple(10, 8, 16),
    make_tuple(10, 9, 18),  make_tuple(10, 11, 11), make_tuple(10, 12, 12),

};
INSTANTIATE_TEST_SUITE_P(RoundUpTest, RoundUpTest,
                         ::testing::ValuesIn(RoundUpValues));
TEST_P(RoundUpTest, RoundUpTest) { ASSERT_EQ(roundUp(in, multiple), expected); }

using roundUpDivisionSafeType = std::tuple<uint64_t, uint64_t, uint64_t>;
class roundUpDivisionSafeTest
    : public ::testing::TestWithParam<roundUpDivisionSafeType> {
protected:
  roundUpDivisionSafeTest() = default;
  virtual void SetUp() {
    in = std::get<0>(GetParam());
    divider = std::get<1>(GetParam());
    expected = std::get<2>(GetParam());
  }

  uint64_t in; // input
  uint64_t divider;
  uint64_t expected; // expected output
};
static const roundUpDivisionSafeType roundUpDivisionSafeValues[] = {
    make_tuple(0, 10, 0),
    make_tuple(10, 10, 1),
    make_tuple(10, 1, 10),
    make_tuple(10, 2, 5),
    make_tuple(10, 3, 4),
    make_tuple(10, 4, 3),
    make_tuple(10, 5, 2),
    make_tuple(10, 6, 2),
    make_tuple(10, 7, 2),
    make_tuple(10, 8, 2),
    make_tuple(10, 9, 2),
    make_tuple(0, 1, 0),
    make_tuple(1, 1, 1),
    make_tuple(numeric_limits<uint64_t>::max() - 1, 1,
               numeric_limits<uint64_t>::max() - 1),
    make_tuple(numeric_limits<uint64_t>::max(), 1,
               numeric_limits<uint64_t>::max()),
    make_tuple(0, numeric_limits<uint64_t>::max() - 1, 0),
    make_tuple(1, numeric_limits<uint64_t>::max() - 1, 1),
    make_tuple(numeric_limits<uint64_t>::max() - 1,
               numeric_limits<uint64_t>::max() - 1, 1),
    make_tuple(numeric_limits<uint64_t>::max(),
               numeric_limits<uint64_t>::max() - 1, 2),
    make_tuple(0, numeric_limits<uint64_t>::max(), 0),
    make_tuple(1, numeric_limits<uint64_t>::max(), 1),
    make_tuple(numeric_limits<uint64_t>::max() - 1,
               numeric_limits<uint64_t>::max(), 1),
    make_tuple(numeric_limits<uint64_t>::max(), numeric_limits<uint64_t>::max(),
               1),

};
INSTANTIATE_TEST_SUITE_P(roundUpDivisionSafeTest, roundUpDivisionSafeTest,
                         ::testing::ValuesIn(roundUpDivisionSafeValues));
TEST_P(roundUpDivisionSafeTest, roundUpDivisionSafeTest) {
  ASSERT_EQ(roundUpDivisionSafe(in, divider), expected);
}

using IsAlignedType = std::tuple<int, int>;
class IsAlignedTest : public ::testing::TestWithParam<IsAlignedType> {
protected:
  IsAlignedTest() = default;
  virtual void SetUp() {
    value = std::get<0>(GetParam());
    multiple = std::get<1>(GetParam());
  }

  int value;
  int multiple;
};
INSTANTIATE_TEST_SUITE_P(IsAlignedTest, IsAlignedTest,
                         ::testing::Combine(::testing::Range(0, 32),
                                            ::testing::Range(0, 32)));
TEST_P(IsAlignedTest, IsAlignedAfterRoundUpTest) {
  ASSERT_TRUE(isAligned(roundUp(value, multiple), multiple));
}

using IsInType = std::tuple<string, bool>;
class IsInTest : public ::testing::TestWithParam<IsInType> {
protected:
  IsInTest() = default;
  virtual void SetUp() {
    in = std::get<0>(GetParam());
    expected = std::get<1>(GetParam());
  }

  string in;     // input
  bool expected; // expected output
};

static const IsInType IsInValues[] = {
    make_tuple("foo", true),    make_tuple("foo2", true),
    make_tuple("bar", true),    make_tuple("baz", true),
    make_tuple("foo1", false),  make_tuple("bar2", false),
    make_tuple("baz-1", false), make_tuple("quz", false),

};
INSTANTIATE_TEST_SUITE_P(IsInTest, IsInTest, ::testing::ValuesIn(IsInValues));
TEST_P(IsInTest, IsInTest) {
  ASSERT_EQ(isIn(in, {"foo", "foo2", "bar", "baz"}), expected);
}

using TrimSpacesType = std::tuple<string, string>;
class TrimSpacesTest : public ::testing::TestWithParam<TrimSpacesType> {
protected:
  TrimSpacesTest() = default;
  virtual void SetUp() {
    in = std::get<0>(GetParam());
    out = std::get<1>(GetParam());
  }

  string in;  // input
  string out; // expected output
};

static const TrimSpacesType TrimSpacesValues[] = {
#define STR "fo2o 3,24 b5a#r"
    make_tuple("foo", "foo"),
    make_tuple(STR, STR),
    make_tuple("  " STR, STR),
    make_tuple("\t" STR, STR),
    make_tuple(" \t " STR, STR),
    make_tuple(STR "  ", STR),
    make_tuple(STR "\t", STR),
    make_tuple(STR "  \t  ", STR),
    make_tuple("  " STR "  ", STR),
    make_tuple("\t" STR "\t", STR),
    make_tuple("  \t  " STR "  \t  ", STR),
    make_tuple("    ", ""),
    make_tuple("  \t", ""),
    make_tuple("  \t  ", ""),
    make_tuple("\t  ", ""),
#undef STR
};
INSTANTIATE_TEST_SUITE_P(TrimSpacesTest, TrimSpacesTest,
                         ::testing::ValuesIn(TrimSpacesValues));
TEST_P(TrimSpacesTest, TrimSpacesTest) { ASSERT_EQ(trimSpaces(in), out); }

using splitStringType = std::tuple<string, char, vector<string>>;
class SplitStringTest : public ::testing::TestWithParam<splitStringType> {
protected:
  SplitStringTest() = default;
  virtual void SetUp() {
    in = std::get<0>(GetParam());
    sep = std::get<1>(GetParam());
    out = std::get<2>(GetParam());
  }

  string in;          // input
  char sep;           // the separator
  vector<string> out; // expected output
};
static const splitStringType splitStringValues[] = {
    make_tuple("", ' ', vector<string>({})),
    make_tuple(" ", ' ', vector<string>({})),
    make_tuple(" ini mi,ni  moe ", ' ',
               vector<string>({"ini", "mi,ni", "moe"})),
    make_tuple(" 412, 542,732 , ", ',',
               vector<string>({" 412", " 542", "732 ", " "})),
    make_tuple("\0 412, 542,732 , ", ',', vector<string>({})),
    make_tuple(" 412, 542\0,732 , ", ',', vector<string>({" 412", " 542"})),

};
INSTANTIATE_TEST_SUITE_P(SplitStringTest, SplitStringTest,
                         ::testing::ValuesIn(splitStringValues));
TEST_P(SplitStringTest, SplitStringTest) {
  auto split = splitString(in, sep);
  ASSERT_EQ(split.size(), out.size());
  ASSERT_TRUE(std::equal(split.begin(), split.end(), out.begin()));
}

TEST(MakeUniqueTest, Test) {
  ASSERT_NO_THROW({
    auto s = std::make_unique<int>(0);
    ASSERT_EQ(*s, 0);
  });
  ASSERT_NO_THROW({
    auto s = std::make_unique<int>(314);
    ASSERT_EQ(*s, 314);
  });
}

using copyPixelsType = std::tuple<int, int, int, int>;
class CopyPixelsTest : public ::testing::TestWithParam<copyPixelsType> {
protected:
  CopyPixelsTest() = default;
  virtual void SetUp() {
    dstPitch = std::get<0>(GetParam());
    srcPitch = std::get<1>(GetParam());
    rowSize = min(min(std::get<2>(GetParam()), srcPitch), dstPitch);
    height = std::get<3>(GetParam());

    assert(srcPitch * height < numeric_limits<uint8_t>::max());
    assert(dstPitch * height < numeric_limits<uint8_t>::max());

    src.resize(static_cast<size_t>(srcPitch) * height);
    dst.resize(static_cast<size_t>(dstPitch) * height);

    fill(src.begin(), src.end(), newVal);
    fill(dst.begin(), dst.end(), origVal);
  }
  void copy() {
    copyPixels(reinterpret_cast<std::byte*>(&(dst[0])), dstPitch,
               reinterpret_cast<const std::byte*>(&(src[0])), srcPitch, rowSize,
               height);
  }
  void compare() {
    for (int y = 0; y < height; y++) {
      for (int x = 0; x < srcPitch; x++)
        ASSERT_EQ(src[y * srcPitch + x], newVal);
      for (int x = 0; x < dstPitch; x++)
        ASSERT_EQ(dst[y * dstPitch + x], x < rowSize ? newVal : origVal);
    }
  }

  static constexpr uint8_t newVal = 0;
  static constexpr uint8_t origVal = -1;
  vector<uint8_t> src;
  vector<uint8_t> dst;
  int dstPitch;
  int srcPitch;
  int rowSize;
  int height;
};
INSTANTIATE_TEST_SUITE_P(CopyPixelsTest, CopyPixelsTest,
                         testing::Combine(testing::Range(1, 4, 1),
                                          testing::Range(1, 4, 1),
                                          testing::Range(1, 4, 1),
                                          testing::Range(1, 4, 1)));
TEST_P(CopyPixelsTest, CopyPixelsTest) {
  copy();
  compare();
}

} // namespace rawspeed_test
