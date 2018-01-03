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

} // namespace rawspeed_test
