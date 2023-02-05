/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2022 Roman Lebedev

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

#include "common/XTransPhase.h"
#include <algorithm>     // for fill, min, copy, equal, fill_n, max
#include <array>         // for array
#include <cassert>       // for assert
#include <cstddef>       // for size_t
#include <cstdint>       // for uint8_t, uint16_t
#include <gtest/gtest.h> // for ParamIteratorInterface, ParamGeneratorInt...
#include <limits>        // for numeric_limits
#include <memory>        // for allocator, make_unique, unique_ptr
#include <string>        // for basic_string, string, operator==
#include <tuple>         // for make_tuple, get, tuple
#include <type_traits>   // for __strip_reference_wrapper<>::__type
#include <vector>        // for vector, vector<>::iterator, vector<>::val...

using rawspeed::CFAColor;
using rawspeed::ColorFilterArray;
using rawspeed::XTransPhase;

namespace rawspeed {

::std::ostream& operator<<(::std::ostream& os, const XTransPhase p) {
  switch (p) {
  case XTransPhase::RGGB:
    return os << "RGGB";
  case XTransPhase::GRBG:
    return os << "GRBG";
  case XTransPhase::GBRG:
    return os << "GBRG";
  case XTransPhase::BGGR:
    return os << "BGGR";
  }
  __builtin_unreachable();
}

} // namespace rawspeed

namespace rawspeed_test {

auto AllKnownCFAColors =
    ::testing::Values(CFAColor::RED, CFAColor::GREEN, CFAColor::BLUE,
                      CFAColor::CYAN, CFAColor::MAGENTA, CFAColor::YELLOW,
                      CFAColor::WHITE, CFAColor::FUJI_GREEN, CFAColor::UNKNOWN);

auto AllKnownXTransPhases = ::testing::Values(
    XTransPhase::RGGB, XTransPhase::GRBG, XTransPhase::GBRG, XTransPhase::BGGR);

auto AllPossible2x2CFAs = ::testing::Combine(
    AllKnownCFAColors, AllKnownCFAColors, AllKnownCFAColors, AllKnownCFAColors);

auto AllPossibleXTransPhaseShifts =
    ::testing::Combine(AllKnownXTransPhases, AllKnownXTransPhases);

static const std::map<std::array<CFAColor, 4>, XTransPhase> KnownXTransCFAs = {
    {{CFAColor::RED, CFAColor::GREEN, CFAColor::GREEN, CFAColor::BLUE},
     XTransPhase::RGGB},
    {{CFAColor::GREEN, CFAColor::RED, CFAColor::BLUE, CFAColor::GREEN},
     XTransPhase::GRBG},
    {{CFAColor::GREEN, CFAColor::BLUE, CFAColor::RED, CFAColor::GREEN},
     XTransPhase::GBRG},
    {{CFAColor::BLUE, CFAColor::GREEN, CFAColor::GREEN, CFAColor::RED},
     XTransPhase::BGGR},
};

template <typename tuple_t>
constexpr auto get_array_from_tuple(tuple_t&& tuple) {
  constexpr auto get_array = [](auto&&... x) {
    return std::array{std::forward<decltype(x)>(x)...};
  };
  return std::apply(get_array, std::forward<tuple_t>(tuple));
}

class XTransPhaseFromCFATest
    : public ::testing::TestWithParam<
          std::tuple<CFAColor, CFAColor, CFAColor, CFAColor>> {
protected:
  XTransPhaseFromCFATest() = default;
  virtual void SetUp() {
    in = get_array_from_tuple(GetParam());
    if (auto it = KnownXTransCFAs.find(in); it != KnownXTransCFAs.end())
      expected = it->second;
    cfa.setCFA({2, 2}, in[0], in[1], in[2], in[3]);
  }

  std::array<CFAColor, 4> in;
  std::optional<XTransPhase> expected;
  ColorFilterArray cfa;
};

INSTANTIATE_TEST_CASE_P(All2x2CFAs, XTransPhaseFromCFATest, AllPossible2x2CFAs);
TEST_P(XTransPhaseFromCFATest, getAsXTransPhaseTest) {
  EXPECT_EQ(expected, rawspeed::getAsXTransPhase(cfa));
}

class XTransPhaseToCFATest : public ::testing::TestWithParam<XTransPhase> {
protected:
  XTransPhaseToCFATest() = default;
  virtual void SetUp() {
    for (auto it : KnownXTransCFAs) {
      if (it.second == GetParam()) {
        in = it.second;
        expected = it.first;
        break;
      }
    }
    assert(in.has_value());
  }

  std::optional<XTransPhase> in;
  std::array<CFAColor, 4> expected;
};

INSTANTIATE_TEST_CASE_P(AllXTransPhases, XTransPhaseToCFATest,
                        AllKnownXTransPhases);
TEST_P(XTransPhaseToCFATest, getAsCFAColorsTest) {
  EXPECT_EQ(expected, rawspeed::getAsCFAColors(*in));
}

class XTransPhaseShifTest
    : public ::testing::TestWithParam<std::tuple<XTransPhase, XTransPhase>> {
protected:
  XTransPhaseShifTest() = default;
  virtual void SetUp() {
    src = std::get<0>(GetParam());
    tgt = std::get<1>(GetParam());
  }

  XTransPhase src;
  XTransPhase tgt;
};
INSTANTIATE_TEST_CASE_P(AllXTransPhaseShifts, XTransPhaseShifTest,
                        AllPossibleXTransPhaseShifts);

struct AbstractElement {};

struct TopLeftElement final : AbstractElement {};
struct TopRightElement final : AbstractElement {};
struct BottomLeftElement final : AbstractElement {};
struct BottomRightElement final : AbstractElement {};

static const TopLeftElement e00;
static const TopRightElement e01;
static const BottomLeftElement e10;
static const BottomRightElement e11;

::std::ostream& operator<<(::std::ostream& os, const AbstractElement* e) {
  if (&e00 == e)
    return os << "e00";
  if (&e01 == e)
    return os << "e01";
  if (&e10 == e)
    return os << "e10";
  if (&e11 == e)
    return os << "e11";
  __builtin_unreachable();
}

static const std::map<XTransPhase, std::array<const AbstractElement*, 4>>
    ExpectedXTransPhaseShifts = {
        {XTransPhase::RGGB, {&e00, &e01, &e10, &e11}}, // baseline
        {XTransPhase::GRBG, {&e01, &e00, &e11, &e10}}, // swap columns
        {XTransPhase::GBRG, {&e10, &e11, &e00, &e01}}, // swap rows
        {XTransPhase::BGGR, {&e11, &e10, &e01, &e00}}, // swap rows and columns
};

TEST_P(XTransPhaseShifTest, applyPhaseShiftTest) {
  EXPECT_EQ(
      ExpectedXTransPhaseShifts.at(tgt),
      rawspeed::applyPhaseShift(ExpectedXTransPhaseShifts.at(src), src, tgt));
}

struct AbstractColorElement {};

struct RedElement final : AbstractColorElement {};
struct FirstGreenElement final : AbstractColorElement {};
struct SecondGreenElement final : AbstractColorElement {};
struct BlueElement final : AbstractColorElement {};

static const RedElement eR;
static const FirstGreenElement eG0;
static const SecondGreenElement eG1;
static const BlueElement eB;

::std::ostream& operator<<(::std::ostream& os, const AbstractColorElement* e) {
  if (&eR == e)
    return os << "eR";
  if (&eG0 == e)
    return os << "eG0";
  if (&eG1 == e)
    return os << "eG1";
  if (&eB == e)
    return os << "eB";
  __builtin_unreachable();
}

static const std::map<XTransPhase, std::array<const AbstractColorElement*, 4>>
    ExpectedXTransStablePhaseShifts = {
        {XTransPhase::RGGB, {&eR, &eG0, &eG1, &eB}}, // baseline
        {XTransPhase::GRBG, {&eG0, &eR, &eB, &eG1}}, // swap columns
        {XTransPhase::GBRG, {&eG0, &eB, &eR, &eG1}}, // swap rows
        {XTransPhase::BGGR, {&eB, &eG0, &eG1, &eR}}, // swap rows and columns
};

TEST_P(XTransPhaseShifTest, applyStablePhaseShiftTest) {
  EXPECT_EQ(ExpectedXTransStablePhaseShifts.at(tgt),
            rawspeed::applyStablePhaseShift(
                ExpectedXTransStablePhaseShifts.at(src), src, tgt));
}

} // namespace rawspeed_test
