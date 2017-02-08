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

#include "common/Common.h" // for clampBits, isIn, isPowerOfTwo, roundUp
#include <algorithm>       // for equal
#include <gtest/gtest.h>   // for get, IsNullLiteralHelper, ParamIteratorIn...
#include <stddef.h>        // for size_t
#include <string>          // for basic_string, string, allocator, operator==
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
    {0, true},  {1, true},  {2, true}, {3, false}, {4, true},   {5, false},
    {6, false}, {7, false}, {8, true}, {9, false}, {10, false}, {11, false}};
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
    {0, 0, 0},   {0, 10, 0},  {10, 0, 10}, {10, 10, 10}, {10, 1, 10},
    {10, 2, 10}, {10, 3, 12}, {10, 4, 12}, {10, 5, 10},  {10, 6, 12},
    {10, 7, 14}, {10, 8, 16}, {10, 9, 18}, {10, 11, 11}, {10, 12, 12}};
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
    {"foo", true},   {"foo2", true},  {"bar", true},    {"baz", true},
    {"foo1", false}, {"bar2", false}, {"baz-1", false}, {"quz", false}};
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

#define ROWS(v, p, pv) {(v), (p), ((v) <= (pv)) ? (v) : (pv)},

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
    {0, 0, 0},  {0, 32, 0},  {32, 0, 0},   {32, 32, 32},
    {32, 2, 3}, {-32, 0, 0}, {-32, 32, 0}, GENERATE()};
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
    {"foo", "foo"},
    {STR, STR},
    {"  " STR, STR},
    {"\t" STR, STR},
    {" \t " STR, STR},
    {STR "  ", STR},
    {STR "\t", STR},
    {STR "  \t  ", STR},
    {"  " STR "  ", STR},
    {"\t" STR "\t", STR},
    {"  \t  " STR "  \t  ", STR},
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
    {" ini mi,ni  moe ", ' ', vector<string>({"ini", "mi,ni", "moe"})},
    {" 412, 542,732 , ", ',', vector<string>({" 412", " 542", "732 ", " "})},
};
INSTANTIATE_TEST_CASE_P(SplitStringTest, SplitStringTest,
                        ::testing::ValuesIn(splitStringValues));
TEST_P(SplitStringTest, SplitStringTest) {
  auto split = splitString(in, sep);
  ASSERT_EQ(split.size(), out.size());
  ASSERT_TRUE(std::equal(split.begin(), split.end(), out.begin()));
}
