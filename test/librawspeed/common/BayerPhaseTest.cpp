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

#include "common/BayerPhase.h"
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

using rawspeed::BayerPhase;
using rawspeed::CFAColor;
using rawspeed::ColorFilterArray;

namespace rawspeed {

::std::ostream& operator<<(::std::ostream& os, const BayerPhase p) {
  switch (p) {
  case BayerPhase::RGGB:
    return os << "RGGB";
  case BayerPhase::GRBG:
    return os << "GRBG";
  case BayerPhase::GBRG:
    return os << "GBRG";
  case BayerPhase::BGGR:
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

auto AllKnownBayerPhases = ::testing::Values(
    BayerPhase::RGGB, BayerPhase::GRBG, BayerPhase::GBRG, BayerPhase::BGGR);

auto AllPossible2x2CFAs = ::testing::Combine(
    AllKnownCFAColors, AllKnownCFAColors, AllKnownCFAColors, AllKnownCFAColors);

auto AllPossibleBayerPhaseShifts =
    ::testing::Combine(AllKnownBayerPhases, AllKnownBayerPhases);

static const std::map<std::array<CFAColor, 4>, BayerPhase> KnownBayerCFAs = {
    {{CFAColor::RED, CFAColor::GREEN, CFAColor::GREEN, CFAColor::BLUE},
     BayerPhase::RGGB},
    {{CFAColor::GREEN, CFAColor::RED, CFAColor::BLUE, CFAColor::GREEN},
     BayerPhase::GRBG},
    {{CFAColor::GREEN, CFAColor::BLUE, CFAColor::RED, CFAColor::GREEN},
     BayerPhase::GBRG},
    {{CFAColor::BLUE, CFAColor::GREEN, CFAColor::GREEN, CFAColor::RED},
     BayerPhase::BGGR},
};

template <typename tuple_t>
constexpr auto get_array_from_tuple(tuple_t&& tuple) {
  constexpr auto get_array = [](auto&&... x) {
    return std::array{std::forward<decltype(x)>(x)...};
  };
  return std::apply(get_array, std::forward<tuple_t>(tuple));
}

class BayerPhaseFromCFATest
    : public ::testing::TestWithParam<
          std::tuple<CFAColor, CFAColor, CFAColor, CFAColor>> {
protected:
  BayerPhaseFromCFATest() = default;
  virtual void SetUp() {
    in = get_array_from_tuple(GetParam());
    if (auto it = KnownBayerCFAs.find(in); it != KnownBayerCFAs.end())
      expected = it->second;
    cfa.setCFA({2, 2}, in[0], in[1], in[2], in[3]);
  }

  std::array<CFAColor, 4> in;
  std::optional<BayerPhase> expected;
  ColorFilterArray cfa;
};

INSTANTIATE_TEST_CASE_P(All2x2CFAs, BayerPhaseFromCFATest, AllPossible2x2CFAs);
TEST_P(BayerPhaseFromCFATest, getAsBayerPhaseTest) {
  EXPECT_EQ(expected, rawspeed::getAsBayerPhase(cfa));
}

class BayerPhaseToCFATest : public ::testing::TestWithParam<BayerPhase> {
protected:
  BayerPhaseToCFATest() = default;
  virtual void SetUp() {
    for (auto it : KnownBayerCFAs) {
      if (it.second == GetParam()) {
        in = it.second;
        expected = it.first;
        break;
      }
    }
    assert(in.has_value());
  }

  std::optional<BayerPhase> in;
  std::array<CFAColor, 4> expected;
};

INSTANTIATE_TEST_CASE_P(AllBayerPhases, BayerPhaseToCFATest,
                        AllKnownBayerPhases);
TEST_P(BayerPhaseToCFATest, getAsCFAColorsTest) {
  EXPECT_EQ(expected, rawspeed::getAsCFAColors(*in));
}

class BayerPhaseShifTest
    : public ::testing::TestWithParam<std::tuple<BayerPhase, BayerPhase>> {
protected:
  BayerPhaseShifTest() = default;
  virtual void SetUp() {
    src = std::get<0>(GetParam());
    tgt = std::get<1>(GetParam());
  }

  BayerPhase src;
  BayerPhase tgt;
};
INSTANTIATE_TEST_CASE_P(AllBayerPhaseShifts, BayerPhaseShifTest,
                        AllPossibleBayerPhaseShifts);

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

static const std::map<BayerPhase, std::array<const AbstractElement*, 4>>
    ExpectedBayerPhaseShifts = {
        {BayerPhase::RGGB, {&e00, &e01, &e10, &e11}}, // baseline
        {BayerPhase::GRBG, {&e01, &e00, &e11, &e10}}, // swap columns
        {BayerPhase::GBRG, {&e10, &e11, &e00, &e01}}, // swap rows
        {BayerPhase::BGGR, {&e11, &e10, &e01, &e00}}, // swap rows and columns
};

TEST_P(BayerPhaseShifTest, applyPhaseShiftTest) {
  EXPECT_EQ(
      ExpectedBayerPhaseShifts.at(tgt),
      rawspeed::applyPhaseShift(ExpectedBayerPhaseShifts.at(src), src, tgt));
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

static const std::map<BayerPhase, std::array<const AbstractColorElement*, 4>>
    ExpectedBayerStablePhaseShifts = {
        {BayerPhase::RGGB, {&eR, &eG0, &eG1, &eB}}, // baseline
        {BayerPhase::GRBG, {&eG0, &eR, &eB, &eG1}}, // swap columns
        {BayerPhase::GBRG, {&eG0, &eB, &eR, &eG1}}, // swap rows
        {BayerPhase::BGGR, {&eB, &eG0, &eG1, &eR}}, // swap rows and columns
};

TEST_P(BayerPhaseShifTest, applyStablePhaseShiftTest) {
  EXPECT_EQ(ExpectedBayerStablePhaseShifts.at(tgt),
            rawspeed::applyStablePhaseShift(
                ExpectedBayerStablePhaseShifts.at(src), src, tgt));
}

} // namespace rawspeed_test
