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

#include "common/Common.h" // for uchar8, clampBits, isIn, isPower...
#include <algorithm>       // for fill, min, equal
#include <cassert>         // for assert
#include <cstddef>         // for size_t
#include <gtest/gtest.h>   // for make_tuple, get, IsNullLiteralHe...
#include <limits>          // for numeric_limits
#include <memory>          // for unique_ptr
#include <string>          // for basic_string, string, allocator
#include <vector>          // for vector

using namespace std;
using namespace RawSpeed;

using powerOfTwoType = std::tr1::tuple<int, bool>;
class PowerOfTwoTest : public ::testing::TestWithParam<powerOfTwoType> {
protected:
  PowerOfTwoTest() = default;
  virtual void SetUp() {
    in = std::tr1::get<0>(GetParam());
    expected = std::tr1::get<1>(GetParam());
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
INSTANTIATE_TEST_CASE_P(PowerOfTwoTest, PowerOfTwoTest,
                        ::testing::ValuesIn(powerOfTwoValues));
TEST_P(PowerOfTwoTest, PowerOfTwoTest) {
  ASSERT_EQ(isPowerOfTwo(in), expected);
}

using RoundUpType = std::tr1::tuple<size_t, size_t, size_t>;
class RoundUpTest : public ::testing::TestWithParam<RoundUpType> {
protected:
  RoundUpTest() = default;
  virtual void SetUp() {
    in = std::tr1::get<0>(GetParam());
    multiple = std::tr1::get<1>(GetParam());
    expected = std::tr1::get<2>(GetParam());
  }

  size_t in; // input
  size_t multiple;
  size_t expected; // expected output
};
static const RoundUpType RoundUpValues[] = {
    make_tuple(0, 0, 0),    make_tuple(0, 10, 0),   make_tuple(10, 0, 10),
    make_tuple(10, 10, 10), make_tuple(10, 1, 10),  make_tuple(10, 2, 10),
    make_tuple(10, 3, 12),  make_tuple(10, 4, 12),  make_tuple(10, 5, 10),
    make_tuple(10, 6, 12),  make_tuple(10, 7, 14),  make_tuple(10, 8, 16),
    make_tuple(10, 9, 18),  make_tuple(10, 11, 11), make_tuple(10, 12, 12),

};
INSTANTIATE_TEST_CASE_P(RoundUpTest, RoundUpTest,
                        ::testing::ValuesIn(RoundUpValues));
TEST_P(RoundUpTest, RoundUpTest) { ASSERT_EQ(roundUp(in, multiple), expected); }

using IsInType = std::tr1::tuple<string, bool>;
class IsInTest : public ::testing::TestWithParam<IsInType> {
protected:
  IsInTest() = default;
  virtual void SetUp() {
    in = std::tr1::get<0>(GetParam());
    expected = std::tr1::get<1>(GetParam());
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
INSTANTIATE_TEST_CASE_P(IsInTest, IsInTest, ::testing::ValuesIn(IsInValues));
TEST_P(IsInTest, IsInTest) {
  ASSERT_EQ(isIn(in, {"foo", "foo2", "bar", "baz"}), expected);
}

using ClampBitsType =
    std::tr1::tuple<long long int, int, unsigned long long int>;
class ClampBitsTest : public ::testing::TestWithParam<ClampBitsType> {
protected:
  ClampBitsTest() = default;
  virtual void SetUp() {
    in = std::tr1::get<0>(GetParam());
    n = std::tr1::get<1>(GetParam());
    expected = std::tr1::get<2>(GetParam());
  }

  long long int in; // input
  int n;
  unsigned long long int expected; // expected output
};

#define ROWS(v, p, pv) make_tuple((v), (p), ((v) <= (pv)) ? (v) : (pv)),

#define THREEROWS(v, p)                                                        \
  ROWS(((1ULL << (v##ULL)) - 1ULL), (p), ((1ULL << (p##ULL)) - 1ULL))          \
  ROWS(((1ULL << (v##ULL)) - 0ULL), (p), ((1ULL << (p##ULL)) - 1ULL))          \
  ROWS(((1ULL << (v##ULL)) + 1ULL), (p), ((1ULL << (p##ULL)) - 1ULL))

#define MOREROWS(v)                                                            \
  THREEROWS(v, 0)                                                              \
  THREEROWS(v, 1)                                                              \
  THREEROWS(v, 2)                                                              \
  THREEROWS(v, 4)                                                              \
  THREEROWS(v, 8) THREEROWS(v, 16) THREEROWS(v, 24) THREEROWS(v, 32)

#define GENERATE()                                                             \
  MOREROWS(0)                                                                  \
  MOREROWS(1)                                                                  \
  MOREROWS(2) MOREROWS(4) MOREROWS(8) MOREROWS(16) MOREROWS(24) MOREROWS(32)

static const ClampBitsType ClampBitsValues[] = {
    make_tuple(0, 0, 0),    make_tuple(0, 32, 0),
    make_tuple(32, 0, 0),   make_tuple(32, 32, 32),
    make_tuple(32, 2, 3),   make_tuple(-32, 0, 0),
    make_tuple(-32, 32, 0), GENERATE()};
INSTANTIATE_TEST_CASE_P(ClampBitsTest, ClampBitsTest,
                        ::testing::ValuesIn(ClampBitsValues));
TEST_P(ClampBitsTest, ClampBitsTest) { ASSERT_EQ(clampBits(in, n), expected); }

using TrimSpacesType = std::tr1::tuple<string, string>;
class TrimSpacesTest : public ::testing::TestWithParam<TrimSpacesType> {
protected:
  TrimSpacesTest() = default;
  virtual void SetUp() {
    in = std::tr1::get<0>(GetParam());
    out = std::tr1::get<1>(GetParam());
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
INSTANTIATE_TEST_CASE_P(TrimSpacesTest, TrimSpacesTest,
                        ::testing::ValuesIn(TrimSpacesValues));
TEST_P(TrimSpacesTest, TrimSpacesTest) { ASSERT_EQ(trimSpaces(in), out); }

using splitStringType = std::tr1::tuple<string, char, vector<string>>;
class SplitStringTest : public ::testing::TestWithParam<splitStringType> {
protected:
  SplitStringTest() = default;
  virtual void SetUp() {
    in = std::tr1::get<0>(GetParam());
    sep = std::tr1::get<1>(GetParam());
    out = std::tr1::get<2>(GetParam());
  }

  string in;          // input
  char sep;           // the separator
  vector<string> out; // expected output
};
static const splitStringType splitStringValues[] = {
    make_tuple(" ini mi,ni  moe ", ' ',
               vector<string>({"ini", "mi,ni", "moe"})),
    make_tuple(" 412, 542,732 , ", ',',
               vector<string>({" 412", " 542", "732 ", " "})),

};
INSTANTIATE_TEST_CASE_P(SplitStringTest, SplitStringTest,
                        ::testing::ValuesIn(splitStringValues));
TEST_P(SplitStringTest, SplitStringTest) {
  auto split = splitString(in, sep);
  ASSERT_EQ(split.size(), out.size());
  ASSERT_TRUE(std::equal(split.begin(), split.end(), out.begin()));
}

TEST(UnrollLoopTest, Test) {
  ASSERT_NO_THROW({
    int cnt = 0;
    unroll_loop<0>([&](int i) { cnt++; });
    ASSERT_EQ(cnt, 0);
  });

  ASSERT_NO_THROW({
    int cnt = 0;
    unroll_loop<3>([&](int i) { cnt++; });
    ASSERT_EQ(cnt, 3);
  });
}

TEST(GetThreadCountTest, Test) {
  ASSERT_NO_THROW({ ASSERT_GE(getThreadCount(), 1); });
}

TEST(MakeUniqueTest, Test) {
  ASSERT_NO_THROW({
    auto s = make_unique<int>(0);
    ASSERT_EQ(*s, 0);
  });
  ASSERT_NO_THROW({
    auto s = make_unique<int>(314);
    ASSERT_EQ(*s, 314);
  });
}

using copyPixelsType = std::tr1::tuple<int, int, int, int>;
class CopyPixelsTest : public ::testing::TestWithParam<copyPixelsType> {
protected:
  CopyPixelsTest() = default;
  virtual void SetUp() {
    dstPitch = std::tr1::get<0>(GetParam());
    srcPitch = std::tr1::get<1>(GetParam());
    rowSize = min(min(std::tr1::get<2>(GetParam()), srcPitch), dstPitch);
    height = std::tr1::get<3>(GetParam());

    assert(srcPitch * height < numeric_limits<uchar8>::max());
    assert(dstPitch * height < numeric_limits<uchar8>::max());

    src.resize((size_t)srcPitch * height);
    dst.resize((size_t)dstPitch * height);

    fill(src.begin(), src.end(), 0);
    fill(dst.begin(), dst.end(), -1);
  }
  void generate() {
    uchar8 v = 0;

    for (int y = 0; y < height; y++) {
      for (int x = 0; x < rowSize; x++, v++) {
        src[y * srcPitch + x] = v;
      }
    }
  }
  void copy() {
    copyPixels(&(dst[0]), dstPitch, &(src[0]), srcPitch, rowSize, height);
  }
  void compare() {
    for (int y = 0; y < height; y++) {
      for (int x = 0; x < rowSize; x++) {
        ASSERT_EQ(dst[y * dstPitch + x], src[y * srcPitch + x]);
      }
    }
  }

  vector<uchar8> src;
  vector<uchar8> dst;
  int dstPitch;
  int srcPitch;
  int rowSize;
  int height;
};
INSTANTIATE_TEST_CASE_P(CopyPixelsTest, CopyPixelsTest,
                        testing::Combine(testing::Range(0, 4, 1),
                                         testing::Range(0, 4, 1),
                                         testing::Range(0, 4, 1),
                                         testing::Range(0, 4, 1)));
TEST_P(CopyPixelsTest, CopyPixelsTest) {
  generate();
  copy();
  compare();
}
