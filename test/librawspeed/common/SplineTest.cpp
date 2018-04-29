/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2018 Roman Lebedev

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

#include "common/Spline.h" // for Spline, Spline<>::value_type
#include "common/Common.h" // for ushort16
#include "common/Point.h"  // for iPoint2D, iPoint2D::value_type
#include <algorithm>       // for generate_n
#include <array>           // for array
#include <cassert>         // for assert
#include <cmath>           // for lround, acos, sin
#include <gtest/gtest.h>   // for make_tuple, ParamIteratorInterface, Message
#include <iterator>        // for begin, end, back_inserter
#include <limits>          // for numeric_limits
#include <ostream>         // for operator<<, basic_ostream::operator<<
#include <stdlib.h>        // for exit
#include <type_traits>     // for __decay_and_strip<>::__type, enable_if_t
#include <vector>          // for vector

using rawspeed::Spline;
using std::make_tuple;

namespace rawspeed {

::std::ostream& operator<<(::std::ostream& os, const iPoint2D p) {
  return os << "(" << p.x << ", " << p.y << ")";
}

} // namespace rawspeed

namespace rawspeed_test {

TEST(SplineStaticTest, DefaultIsUshort16) {
  static_assert(std::is_same<Spline<>::value_type, rawspeed::ushort16>::value,
                "wrong default type");
}

#ifndef NDEBUG
TEST(SplineDeathTest, AtLeastTwoPoints) {
  ASSERT_DEATH(
      {
        Spline<> s({});
        s.calculateCurve();
      },
      "at least two points");
  ASSERT_DEATH(
      {
        Spline<> s({{0, 0}});
        s.calculateCurve();
      },
      "at least two points");
  ASSERT_EXIT(
      {
        Spline<> s({{0, {}}, {65535, {}}});
        s.calculateCurve();
        exit(0);
      },
      ::testing::ExitedWithCode(0), "");
}

TEST(SplineDeathTest, XIsFullRange) {
  ASSERT_DEATH(
      {
        Spline<> s({{1, {}}, {65535, {}}});
        s.calculateCurve();
      },
      "front.*0");
  ASSERT_DEATH(
      {
        Spline<> s({{0, {}}, {65534, {}}});
        s.calculateCurve();
      },
      "back.*65535");
}

TEST(SplineDeathTest, YIsLimited) {
  ASSERT_DEATH(
      {
        Spline<> s({{0, {}}, {32767, -1}, {65535, {}}});
        s.calculateCurve();
      },
      "y >= .*min");
  ASSERT_DEATH(
      {
        Spline<> s({{0, {}}, {32767, 65536}, {65535, {}}});
        s.calculateCurve();
      },
      "y <= .*max");
}

TEST(SplineDeathTest, XIsStrictlyIncreasing) {
  ASSERT_DEATH(
      {
        Spline<> s({{0, {}}, {0, {}}, {65535, {}}});
        s.calculateCurve();
      },
      "strictly increasing");
  ASSERT_DEATH(
      {
        Spline<> s({{0, {}}, {32767, {}}, {32767, {}}, {65535, {}}});
        s.calculateCurve();
      },
      "strictly increasing");
  ASSERT_DEATH(
      {
        Spline<> s({{0, {}}, {65535, {}}, {65535, {}}});
        s.calculateCurve();
      },
      "strictly increasing");
  ASSERT_DEATH(
      {
        Spline<> s({{0, {}}, {32767, {}}, {32766, {}}, {65535, {}}});
        s.calculateCurve();
      },
      "strictly increasing");
}
#endif

TEST(SplineDeathTest, ClampUshort16Min) {
  ASSERT_EXIT(
      {
        Spline<> s({{0, 0},
                    {2, 0},
                    {54, 0},
                    {128, 0},
                    {256, 0},
                    {21504, 0},
                    {32768, 0},
                    {57088, 0},
                    {65535, 65535}});
        s.calculateCurve();
        // for x = 484, this used to compute -1.0046832758220527,
        // which is unrepresentable in ushort16.
        exit(0);
      },
      ::testing::ExitedWithCode(0), "");
}
TEST(SplineDeathTest, ClampUshort16Max) {
  ASSERT_EXIT(
      {
        Spline<> s({{0, 0},
                    {2, 0},
                    {56, 0},
                    {128, 0},
                    {256, 0},
                    {21504, 0},
                    {32768, 0},
                    {57088, 0},
                    {65535, 65535}});
        s.calculateCurve();
        // for x = 65535, this used to compute 65535.000000000007,
        // which is unrepresentable in ushort16.
        exit(0);
      },
      ::testing::ExitedWithCode(0), "");
}

using identityType = std::tuple<std::array<rawspeed::iPoint2D, 2>,
                                std::vector<std::array<double, 4>>>;
template <typename T>
class IdentityTest : public ::testing::TestWithParam<identityType> {
protected:
  IdentityTest() = default;

  void CheckSegments() const {
    ASSERT_EQ(gotSegments.size(), expSegments.size());
    for (auto i = 0U; i < gotSegments.size(); i++) {
      const auto s0 = gotSegments[i];
      const auto s1 = expSegments[i];
      ASSERT_EQ(s0.a, s1[0]);
      ASSERT_EQ(s0.b, s1[1]);
      ASSERT_EQ(s0.c, s1[2]);
      ASSERT_EQ(s0.d, s1[3]);
    }
  }

  virtual void SetUp() {
    const auto p = GetParam();
    edges = std::get<0>(p);
    expSegments = std::get<1>(p);

    Spline<T> s({std::begin(edges), std::end(edges)});
    gotSegments = s.getSegments();
    interpolated = s.calculateCurve();

    ASSERT_FALSE(interpolated.empty());
    ASSERT_EQ(interpolated.size(), 65536);
  }

  std::array<rawspeed::iPoint2D, 2> edges;
  std::vector<std::array<double, 4>> expSegments;
  std::vector<typename Spline<T>::Segment> gotSegments;
  std::vector<T> interpolated;
};
static const identityType identityValues[] = {
    make_tuple(std::array<rawspeed::iPoint2D, 2>{{{0, 0}, {65535, 65535}}},
               std::vector<std::array<double, 4>>{{{0.0, 1.0, 0.0, 0.0}}}),
    make_tuple(
        std::array<rawspeed::iPoint2D, 2>{{{0, 65535}, {65535, 0}}},
        std::vector<std::array<double, 4>>{{{65535.0, -1.0, 0.0, 0.0}}})};

using IntegerIdentityTest = IdentityTest<rawspeed::ushort16>;
INSTANTIATE_TEST_CASE_P(IntegerIdentityTest, IntegerIdentityTest,
                        ::testing::ValuesIn(identityValues));
TEST_P(IntegerIdentityTest, ValuesAreLinearlyInterpolated) {
  for (auto x = edges.front().y; x < edges.back().y; ++x)
    ASSERT_EQ(interpolated[x], x);
}
TEST_P(IntegerIdentityTest, SegmentCoeffients) { CheckSegments(); }

using DoubleIdentityTest = IdentityTest<double>;
INSTANTIATE_TEST_CASE_P(DoubleIdentityTest, DoubleIdentityTest,
                        ::testing::ValuesIn(identityValues));
TEST_P(DoubleIdentityTest, ValuesAreLinearlyInterpolated) {
  for (auto x = edges.front().y; x < edges.back().y; ++x) {
    ASSERT_DOUBLE_EQ(interpolated[x], x);
    ASSERT_EQ(interpolated[x], x);
  }
}
TEST_P(DoubleIdentityTest, SegmentCoeffients) { CheckSegments(); }

template <typename T> T lerp(T v0, T v1, T t) {
  return (1.0 - t) * v0 + t * v1;
}

template <typename T = int,
          typename = std::enable_if_t<std::is_arithmetic<T>::value>>
std::vector<T> calculateSteps(int numCp) {
  std::vector<T> steps;

  const auto ptsTotal = 2U + numCp;
  steps.reserve(ptsTotal);

  std::generate_n(std::back_inserter(steps), ptsTotal,
                  [ptsTotal, &steps]() -> T {
                    const double t = double(steps.size()) / (ptsTotal - 1);
                    const double x = lerp(0.0, 65535.0, t);
                    if (std::is_floating_point<T>::value)
                      return x;
                    return std::lround(x);
                  });

  assert(ptsTotal == steps.size());
  return steps;
}
TEST(CalculateStepsEdgesTest, IdentityTest) {
  const int steps = 65534;
  const auto pts = calculateSteps(steps);
  for (auto x = 0U; x < pts.size(); x++)
    ASSERT_EQ(pts[x], x);
}

class CalculateStepsEdgesTest : public ::testing::TestWithParam<int> {
protected:
  CalculateStepsEdgesTest() = default;
  virtual void SetUp() {
    extraSteps = GetParam();
    got = calculateSteps(extraSteps);
  }

  int extraSteps;
  std::vector<int> got;
};
INSTANTIATE_TEST_CASE_P(CalculateStepsEdgesTest, CalculateStepsEdgesTest,
                        ::testing::Range(0, 254));
TEST_P(CalculateStepsEdgesTest, Count) {
  ASSERT_EQ(got.size(), 2 + extraSteps);
}
TEST_P(CalculateStepsEdgesTest, EdgesAreProper) {
  ASSERT_EQ(got.front(), 0);
  ASSERT_EQ(got.back(), 65535);
}

using calculateStepsType = std::tuple<int, std::vector<int>>;

template <typename T>
class CalculateStepsTest : public ::testing::TestWithParam<calculateStepsType> {
protected:
  CalculateStepsTest() = default;
  virtual void SetUp() {
    const auto p = GetParam();
    extraSteps = std::get<0>(p);
    expected = std::get<1>(p);

    got = calculateSteps<T>(extraSteps);
  }

  int extraSteps;
  std::vector<int> expected;
  std::vector<T> got;
};
static const calculateStepsType calculateStepsValues[] = {
    // clang-format off
    make_tuple(0, std::vector<int>{0, 65535}),
    make_tuple(1, std::vector<int>{0, 32768, 65535}),
    make_tuple(2, std::vector<int>{0, 21845, 43690, 65535}),
    make_tuple(3, std::vector<int>{0, 16384, 32768, 49151, 65535}),
    make_tuple(4, std::vector<int>{0, 13107, 26214, 39321, 52428, 65535}),
    make_tuple(5, std::vector<int>{0, 10923, 21845, 32768, 43690, 54613, 65535}),
    make_tuple(6, std::vector<int>{0, 9362, 18724, 28086, 37449, 46811, 56173, 65535}),
    make_tuple(7, std::vector<int>{0, 8192, 16384, 24576, 32768, 40959, 49151, 57343, 65535}),
    make_tuple(8, std::vector<int>{0, 7282, 14563, 21845, 29127, 36408, 43690, 50972, 58253, 65535}),
    // clang-format on
};

using DoubleCalculateStepsTest = CalculateStepsTest<double>;
INSTANTIATE_TEST_CASE_P(CalculateStepsTest, DoubleCalculateStepsTest,
                        ::testing::ValuesIn(calculateStepsValues));
TEST_P(DoubleCalculateStepsTest, Count) {
  ASSERT_EQ(expected.size(), got.size());
  ASSERT_EQ(got.size(), 2 + extraSteps);
}
TEST_P(DoubleCalculateStepsTest, GotExpectedOutput) {
  for (auto i = 0U; i < got.size(); i++) {
    ASSERT_NEAR(got[i], expected[i], 0.5);

    // Check that rounding halfway cases away from zero will work
    ASSERT_GE(got[i], expected[i] - 0.5);
    ASSERT_LT(got[i], expected[i] + 0.5);
  }
}

using IntegerCalculateStepsTest = CalculateStepsTest<int>;
INSTANTIATE_TEST_CASE_P(CalculateStepsTest, IntegerCalculateStepsTest,
                        ::testing::ValuesIn(calculateStepsValues));
TEST_P(IntegerCalculateStepsTest, Count) {
  ASSERT_EQ(expected.size(), got.size());
  ASSERT_EQ(got.size(), 2 + extraSteps);
}
TEST_P(IntegerCalculateStepsTest, GotExpectedOutput) {
  ASSERT_EQ(got, expected);
}

using constantType = std::tuple<int, int>;
template <typename T>
class ConstantTest : public ::testing::TestWithParam<constantType> {
protected:
  ConstantTest() = default;

  void calculateEdges() {
    const auto steps = calculateSteps(numCp);
    edges.reserve(steps.size());

    for (const int step : steps)
      edges.emplace_back(step, constant);

    assert(steps.size() == edges.size());
  }

  void CheckSegments() const {
    for (auto i = 0U; i < gotSegments.size(); i++) {
      const auto s0 = gotSegments[i];
      ASSERT_EQ(s0.a, constant);
      ASSERT_EQ(s0.b, 0);
      ASSERT_EQ(s0.c, 0);
      ASSERT_EQ(s0.d, 0);
    }
  }

  virtual void SetUp() {
    auto p = GetParam();

    constant = std::get<0>(p);
    numCp = std::get<1>(p);

    calculateEdges();

    // EXPECT_TRUE(false) << ::testing::PrintToString((edges));

    Spline<T> s({std::begin(edges), std::end(edges)});
    gotSegments = s.getSegments();
    interpolated = s.calculateCurve();

    ASSERT_FALSE(interpolated.empty());
    ASSERT_EQ(interpolated.size(), 65536);
  }

  int constant;
  int numCp;

  std::vector<rawspeed::iPoint2D> edges;
  std::vector<typename Spline<T>::Segment> gotSegments;
  std::vector<T> interpolated;
};

constexpr auto NumExtraSteps = 3;
static const auto constantValues =
    ::testing::Combine(::testing::ValuesIn(calculateSteps(NumExtraSteps)),
                       ::testing::Range(0, 1 + NumExtraSteps));

using IntegerConstantTest = ConstantTest<rawspeed::ushort16>;
INSTANTIATE_TEST_CASE_P(IntegerConstantTest, IntegerConstantTest,
                        constantValues);
TEST_P(IntegerConstantTest, AllValuesAreEqual) {
  for (const auto value : interpolated)
    ASSERT_EQ(value, constant);
}
TEST_P(IntegerConstantTest, SegmentCoeffients) { CheckSegments(); }

using DoubleConstantTest = ConstantTest<double>;
INSTANTIATE_TEST_CASE_P(DoubleConstantTest, DoubleConstantTest, constantValues);
TEST_P(DoubleConstantTest, AllValuesAreEqual) {
  for (const auto value : interpolated) {
    ASSERT_DOUBLE_EQ(value, constant);
    ASSERT_EQ(value, constant);
  }
}
TEST_P(DoubleConstantTest, SegmentCoeffients) { CheckSegments(); }

class AbstractReferenceTest {
public:
  using T = long double;

  const T xMax = 65535;
  const T yMax = std::numeric_limits<rawspeed::iPoint2D::value_type>::max();

  virtual T calculateRefVal(int x) const = 0;

  virtual ~AbstractReferenceTest() = default;
};

template <int mul, int div>
class SinReferenceTest : public AbstractReferenceTest {
public:
  T calculateRefVal(int x) const final {
    const T pi = std::acos(T(-1));
    const T x2arg = T(mul) * pi / (div * xMax);

    return yMax * std::sin(x2arg * T(x));
  }

  virtual ~SinReferenceTest() = default;
};

using referenceType = std::tuple<int, long double>;

template <typename Tb>
class ReferenceTest : public Tb,
                      public ::testing::TestWithParam<referenceType> {
protected:
  using T = AbstractReferenceTest::T;

  ReferenceTest() = default;

  void calculateReference() {
    const auto xPoints = calculateSteps(numPts);

    reference.reserve(xPoints.size());
    for (const auto xPoint : xPoints)
      reference.emplace_back(xPoint, this->calculateRefVal(xPoint));

    assert(reference.size() == xPoints.size());
  }

  virtual void SetUp() {
    const auto p = GetParam();

    numPts = std::get<0>(p);
    absError = std::get<1>(p);

    calculateReference();

    Spline<T> s(reference);
    interpolated = s.calculateCurve();
    ASSERT_FALSE(interpolated.empty());
    ASSERT_EQ(interpolated.size(), AbstractReferenceTest::xMax + 1);
  }

  void check() const {
    for (auto x = reference.front().x; x < reference.back().x; ++x) {
      const T referen = this->calculateRefVal(x) / this->yMax;
      const T interpo = T(interpolated[x]) / this->yMax;
      ASSERT_NEAR(interpo, referen, absError);
    }
  }

  int numPts;
  long double absError;

  std::vector<rawspeed::iPoint2D> reference;
  std::vector<T> interpolated;
};

using Sin2PiRefTest = ReferenceTest<SinReferenceTest<2, 1>>;
static const referenceType sin2PiRefValues[] = {
    // clang-format off
    make_tuple(0,    1.0E-00),
    make_tuple(1,    1.0E+01), // FIXME: should be 1.0E-00
    make_tuple(2,    1.0E-00),
    make_tuple(3,    1.0E-01),
    make_tuple(4,    1.0E-02),
    make_tuple(5,    1.0E-02),
    make_tuple(6,    1.0E-02),
    make_tuple(7,    1.0E-02),
    make_tuple(8,    1.0E-03),
    make_tuple(9,    1.0E-03),
    make_tuple(10,   1.0E-03),
    make_tuple(11,   1.0E-03),
    make_tuple(12,   1.0E-03),
    make_tuple(13,   1.0E-03),
    make_tuple(14,   1.0E-04),
    // clang-format on
};
INSTANTIATE_TEST_CASE_P(Sin2Pi, Sin2PiRefTest,
                        ::testing::ValuesIn(sin2PiRefValues));
TEST_P(Sin2PiRefTest, NearlyMatchesReference) { check(); }

using SinPiRefTest = ReferenceTest<SinReferenceTest<1, 1>>;
static const referenceType sinPiRefValues[] = {
    // clang-format off
    make_tuple(0,  1.0E-00),
    make_tuple(1,  1.0E-01),
    make_tuple(2,  1.0E-02),
    make_tuple(3,  1.0E-02),
    make_tuple(4,  1.0E-03),
    make_tuple(5,  1.0E-03),
    make_tuple(6,  1.0E-03),
    make_tuple(7,  1.0E-04),
    make_tuple(8,  1.0E-04),
    make_tuple(9,  1.0E-04),
    make_tuple(10, 1.0E-04),
    make_tuple(11, 1.0E-04),
    make_tuple(12, 1.0E-05),
    // clang-format on
};
INSTANTIATE_TEST_CASE_P(SinPi, SinPiRefTest,
                        ::testing::ValuesIn(sinPiRefValues));
TEST_P(SinPiRefTest, NearlyMatchesReference) { check(); }

} // namespace rawspeed_test
