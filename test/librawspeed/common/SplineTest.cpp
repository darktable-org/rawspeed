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

#include "common/Spline.h" // for Spline
#include <array>           // for array
#include <cmath>           // for acos
#include <gtest/gtest.h>   // for AssertionResult, DeathTest, Test, AssertHe...
#include <type_traits>     // for is_same

using rawspeed::Spline;

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
  ASSERT_DEATH({ Spline<>::calculateCurve({}); }, "at least two points");
  ASSERT_DEATH({ Spline<>::calculateCurve({{0, {}}}); }, "at least two points");
  ASSERT_EXIT(
      {
        Spline<>::calculateCurve({{0, {}}, {65535, {}}});
        exit(0);
      },
      ::testing::ExitedWithCode(0), "");
}

TEST(SplineDeathTest, XIsFullRange) {
  ASSERT_DEATH(
      {
        Spline<>::calculateCurve({{1, {}}, {65535, {}}});
      },
      "front.*0");
  ASSERT_DEATH(
      {
        Spline<>::calculateCurve({{0, {}}, {65534, {}}});
      },
      "back.*65535");
}

TEST(SplineDeathTest, YIsLimited) {
  ASSERT_DEATH(
      {
        Spline<>::calculateCurve({{0, {}}, {32767, -1}, {65535, {}}});
      },
      "y >= .*min");
  ASSERT_DEATH(
      {
        Spline<>::calculateCurve({{0, {}}, {32767, 65536}, {65535, {}}});
      },
      "y <= .*max");
}

TEST(SplineDeathTest, XIsStrictlyIncreasing) {
  ASSERT_DEATH(
      {
        Spline<>::calculateCurve({{0, {}}, {0, {}}, {65535, {}}});
      },
      "strictly increasing");
  ASSERT_DEATH(
      {
        Spline<>::calculateCurve(
            {{0, {}}, {32767, {}}, {32767, {}}, {65535, {}}});
      },
      "strictly increasing");
  ASSERT_DEATH(
      {
        Spline<>::calculateCurve({{0, {}}, {65535, {}}, {65535, {}}});
      },
      "strictly increasing");
  ASSERT_DEATH(
      {
        Spline<>::calculateCurve(
            {{0, {}}, {32767, {}}, {32766, {}}, {65535, {}}});
      },
      "strictly increasing");
}
#endif

using identityType = std::array<rawspeed::iPoint2D, 2>;
template <typename T>
class IdentityTest : public ::testing::TestWithParam<identityType> {
protected:
  IdentityTest() = default;
  virtual void SetUp() {
    edges = GetParam();

    interpolated =
        Spline<T>::calculateCurve({std::begin(edges), std::end(edges)});

    ASSERT_FALSE(interpolated.empty());
    ASSERT_EQ(interpolated.size(), 65536);
  }

  std::array<rawspeed::iPoint2D, 2> edges;
  std::vector<T> interpolated;
};
static const identityType identityValues[] = {
    {{{0, 0}, {65535, 65535}}},
    {{{0, 65535}, {65535, 0}}},
};

using IntegerIdentityTest = IdentityTest<rawspeed::ushort16>;
INSTANTIATE_TEST_CASE_P(IntegerIdentityTest, IntegerIdentityTest,
                        ::testing::ValuesIn(identityValues));
TEST_P(IntegerIdentityTest, ValuesAreLinearlyInterpolated) {
  for (auto x = edges.front().y; x < edges.back().y; ++x)
    ASSERT_EQ(interpolated[x], x);
}

using DoubleIdentityTest = IdentityTest<double>;
INSTANTIATE_TEST_CASE_P(DoubleIdentityTest, DoubleIdentityTest,
                        ::testing::ValuesIn(identityValues));
TEST_P(DoubleIdentityTest, ValuesAreLinearlyInterpolated) {
  for (auto x = edges.front().y; x < edges.back().y; ++x) {
    ASSERT_DOUBLE_EQ(interpolated[x], x);
    ASSERT_EQ(interpolated[x], x);
  }
}

template <typename T> T lerp(T v0, T v1, T t) {
  return (1.0 - t) * v0 + t * v1;
}

std::vector<int> calculateSteps(int numCp) {
  std::vector<int> steps;

  const auto ptsTotal = 2U + numCp;
  steps.reserve(ptsTotal);

  const auto dt = 1.0 / (ptsTotal - 1);
  double t = 0.0;
  std::generate_n(std::back_inserter(steps), ptsTotal, [dt, &t]() {
    const double x = lerp(0.0, 65535.0, t);
    t += dt;
    return x + 0.5;
  });

  assert(ptsTotal == steps.size());
  return steps;
}
TEST(CalculateStepsTest, SimpleTest) {
  {
    const int steps = 0;
    const std::vector<int> res{0, 65535};
    ASSERT_EQ(calculateSteps(steps), res);
  }
  {
    const int steps = 1;
    const std::vector<int> res{0, 32768, 65535};
    ASSERT_EQ(calculateSteps(steps), res);
  }
  {
    for (auto steps = 0; steps < 255; steps++) {
      const auto pts = calculateSteps(steps);
      ASSERT_EQ(pts.front(), 0) << "Where steps = " << steps;
      ASSERT_EQ(pts.back(), 65535) << "Where steps = " << steps;
    }
  }
  {
    const int steps = 65534;
    const auto pts = calculateSteps(steps);
    for (auto x = 0U; x < pts.size(); x++)
      ASSERT_EQ(pts[x], x);
  }
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

  virtual void SetUp() {
    auto p = GetParam();

    constant = std::get<0>(p);
    numCp = std::get<1>(p);

    calculateEdges();

    // EXPECT_TRUE(false) << ::testing::PrintToString((edges));

    interpolated =
        Spline<T>::calculateCurve({std::begin(edges), std::end(edges)});

    ASSERT_FALSE(interpolated.empty());
    ASSERT_EQ(interpolated.size(), 65536);
  }

  int constant;
  int numCp;

  std::vector<rawspeed::iPoint2D> edges;
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

using DoubleConstantTest = ConstantTest<double>;
INSTANTIATE_TEST_CASE_P(DoubleConstantTest, DoubleConstantTest, constantValues);
TEST_P(DoubleConstantTest, AllValuesAreEqual) {
  for (const auto value : interpolated) {
    ASSERT_DOUBLE_EQ(value, constant);
    ASSERT_EQ(value, constant);
  }
}

class AbstractReferenceTest {
public:
  using T = long double;

  static constexpr T xMax = 65535;
  static constexpr T yMax =
      std::numeric_limits<rawspeed::iPoint2D::value_type>::max();

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
const AbstractReferenceTest::T AbstractReferenceTest::xMax;
const AbstractReferenceTest::T AbstractReferenceTest::yMax;

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

    interpolated = Spline<T>::calculateCurve(reference);
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
    {0,    1.0E-00},
    {1,    1.0E+01},
    {2,    1.0E-00},
    {3,    1.0E-01},
    {4,    1.0E-02},
    {5,    1.0E-02},
    {6,    1.0E-02},
    {7,    1.0E-02},
    {8,    1.0E-03},
    {9,    1.0E-03},
    {10,   1.0E-03},
    {23,   1.0E-04},
    {48,   1.0E-06},
    {98,   1.0E-07},
    {248,  1.0E-08},
    {498,  1.0E-09},
    {998,  1.0E-09},
    {9998, 1.0E-09},
    // clang-format on
};
INSTANTIATE_TEST_CASE_P(Sin2Pi, Sin2PiRefTest,
                        ::testing::ValuesIn(sin2PiRefValues));
TEST_P(Sin2PiRefTest, NearlyMatchesReference) { check(); }

using SinPi2RefTest = ReferenceTest<SinReferenceTest<1, 2>>;
static const referenceType sinPi2RefValues[] = {
    // clang-format off
    {0,    1.0E-00},
    {1,    1.0E-01},
    {2,    1.0E-01},
    {3,    1.0E-02},
    {4,    1.0E-02},
    {5,    1.0E-02},
    {6,    1.0E-02},
    {7,    1.0E-02},
    {8,    1.0E-02},
    {9,    1.0E-02},
    {10,   1.0E-02},
    {23,   1.0E-03},
    {48,   1.0E-04},
    {98,   1.0E-04},
    {248,  1.0E-05},
    {498,  1.0E-06},
    {998,  1.0E-06},
    {9998, 1.0E-08},
    // clang-format on
};
INSTANTIATE_TEST_CASE_P(SinPi2, SinPi2RefTest,
                        ::testing::ValuesIn(sinPi2RefValues));
TEST_P(SinPi2RefTest, NearlyMatchesReference) { check(); }

} // namespace rawspeed_test
