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
#include <gtest/gtest.h>   // for AssertionResult, DeathTest, Test, AssertHe...

using rawspeed::Spline;

namespace rawspeed_test {

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

TEST(SplineTest, IntegerIdentityTest) {
  const auto s = Spline<>::calculateCurve({{0, 0}, {65535, 65535}});
  ASSERT_FALSE(s.empty());
  ASSERT_EQ(s.size(), 65536);
  for (auto x = 0U; x < s.size(); ++x)
    ASSERT_EQ(s[x], x);
}

TEST(SplineTest, IntegerReverseIdentityTest) {
  const auto s = Spline<>::calculateCurve({{0, 65535}, {65535, 0}});
  ASSERT_FALSE(s.empty());
  ASSERT_EQ(s.size(), 65536);
  for (auto x = 0U; x < s.size(); ++x) {
    ASSERT_EQ(s[x], 65535 - x) << "    Where x is: " << x;
  }
}

} // namespace rawspeed_test
