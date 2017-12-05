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

#include "common/Point.h" // for iPoint2D
#include <gtest/gtest.h>  // for make_tuple, AssertionResult, IsNullLiteral...
#include <limits>         // for numeric_limits
#include <ostream>        // for operator<<, basic_ostream::operator<<, ost...
#include <utility>        // for make_pair, pair, move

using rawspeed::iPoint2D;
using std::make_pair;
using std::move;
using std::numeric_limits;
using std::pair;
using std::tuple;

namespace rawspeed {

::std::ostream& operator<<(::std::ostream& os, const iPoint2D p) {
  return os << "(" << p.x << ", " << p.y << ")";
}

} // namespace rawspeed

namespace rawspeed_test {

static constexpr iPoint2D::area_type maxVal =
    numeric_limits<iPoint2D::value_type>::max();
static constexpr iPoint2D::area_type minVal =
    numeric_limits<iPoint2D::value_type>::min();
static constexpr iPoint2D::area_type absMinVal = -minVal;
static constexpr iPoint2D::area_type maxAreaVal = maxVal * maxVal;
static constexpr iPoint2D::area_type minAreaVal = absMinVal * absMinVal;
static constexpr iPoint2D::area_type mixAreaVal = maxVal * absMinVal;

TEST(PointTest, Constructor) {
  int x = -10, y = 15;
  ASSERT_NO_THROW({
    iPoint2D a;
    ASSERT_EQ(a.x, 0);
    ASSERT_EQ(a.y, 0);
  });
  ASSERT_NO_THROW({
    iPoint2D a(x, y);
    ASSERT_EQ(a.x, x);
    ASSERT_EQ(a.y, y);
  });
  ASSERT_NO_THROW({
    const iPoint2D a(x, y);
    iPoint2D b(a);
    ASSERT_EQ(b.x, x);
    ASSERT_EQ(b.y, y);
  });
  ASSERT_NO_THROW({
    iPoint2D a(x, y);
    iPoint2D b(a);
    ASSERT_EQ(b.x, x);
    ASSERT_EQ(b.y, y);
  });
  ASSERT_NO_THROW({
    const iPoint2D a(x, y);
    iPoint2D b(move(a));
    ASSERT_EQ(b.x, x);
    ASSERT_EQ(b.y, y);
  });
  ASSERT_NO_THROW({
    iPoint2D a(x, y);
    iPoint2D b(move(a));
    ASSERT_EQ(b.x, x);
    ASSERT_EQ(b.y, y);
  });
}

TEST(PointTest, AssignmentConstructor) {
  int x = -10, y = 15;
  ASSERT_NO_THROW({
    iPoint2D a(x, y);
    iPoint2D b(666, 777);
    b = a;
    ASSERT_EQ(b.x, x);
    ASSERT_EQ(b.y, y);
  });
  ASSERT_NO_THROW({
    const iPoint2D a(x, y);
    iPoint2D b(666, 777);
    b = a;
    ASSERT_EQ(b.x, x);
    ASSERT_EQ(b.y, y);
  });
  ASSERT_NO_THROW({
    iPoint2D a(x, y);
    iPoint2D b(666, 777);
    b = move(a);
    ASSERT_EQ(b.x, x);
    ASSERT_EQ(b.y, y);
  });
  ASSERT_NO_THROW({
    const iPoint2D a(x, y);
    iPoint2D b(666, 777);
    b = move(a);
    ASSERT_EQ(b.x, x);
    ASSERT_EQ(b.y, y);
  });
}

TEST(PointTest, EqualityOperator) {
  ASSERT_NO_THROW({
    const iPoint2D a(18, -12);
    const iPoint2D b(18, -12);
    ASSERT_EQ(a, b);
    ASSERT_EQ(b, a);
  });
}

TEST(PointTest, NonEqualityOperator) {
  ASSERT_NO_THROW({
    const iPoint2D a(777, 888);
    const iPoint2D b(888, 777);
    const iPoint2D c(128, 256);
    ASSERT_NE(a, b);
    ASSERT_NE(b, a);
    ASSERT_NE(a, c);
    ASSERT_NE(c, a);
    ASSERT_NE(b, c);
    ASSERT_NE(c, b);
  });
}

using IntPair = pair<int, int>;
using Six = std::tr1::tuple<IntPair, IntPair, IntPair>;
class PointTest : public ::testing::TestWithParam<Six> {
protected:
  PointTest() = default;
  virtual void SetUp() {
    auto p = GetParam();

    auto pair = std::tr1::get<0>(p);
    a = iPoint2D(pair.first, pair.second);

    pair = std::tr1::get<1>(p);
    b = iPoint2D(pair.first, pair.second);

    pair = std::tr1::get<2>(p);
    c = iPoint2D(pair.first, pair.second);
  }

  iPoint2D a;
  iPoint2D b;
  iPoint2D c;
};

/*
#!/bin/bash
for i in -5 0 5;
do
  for j in -5 0 5;
  do
    for k in -5 0 5;
    do
      for q in -5 0 5;
      do
        echo "make_tuple(make_pair($i, $j), make_pair($k, $q),
make_pair($(($i+$k)), $(($j+$q)))),";
      done;
    done;
  done;
done;
*/
static const Six valueSum[]{
    make_tuple(make_pair(-5, -5), make_pair(-5, -5), make_pair(-10, -10)),
    make_tuple(make_pair(-5, -5), make_pair(-5, 0), make_pair(-10, -5)),
    make_tuple(make_pair(-5, -5), make_pair(-5, 5), make_pair(-10, 0)),
    make_tuple(make_pair(-5, -5), make_pair(0, -5), make_pair(-5, -10)),
    make_tuple(make_pair(-5, -5), make_pair(0, 0), make_pair(-5, -5)),
    make_tuple(make_pair(-5, -5), make_pair(0, 5), make_pair(-5, 0)),
    make_tuple(make_pair(-5, -5), make_pair(5, -5), make_pair(0, -10)),
    make_tuple(make_pair(-5, -5), make_pair(5, 0), make_pair(0, -5)),
    make_tuple(make_pair(-5, -5), make_pair(5, 5), make_pair(0, 0)),
    make_tuple(make_pair(-5, 0), make_pair(-5, -5), make_pair(-10, -5)),
    make_tuple(make_pair(-5, 0), make_pair(-5, 0), make_pair(-10, 0)),
    make_tuple(make_pair(-5, 0), make_pair(-5, 5), make_pair(-10, 5)),
    make_tuple(make_pair(-5, 0), make_pair(0, -5), make_pair(-5, -5)),
    make_tuple(make_pair(-5, 0), make_pair(0, 0), make_pair(-5, 0)),
    make_tuple(make_pair(-5, 0), make_pair(0, 5), make_pair(-5, 5)),
    make_tuple(make_pair(-5, 0), make_pair(5, -5), make_pair(0, -5)),
    make_tuple(make_pair(-5, 0), make_pair(5, 0), make_pair(0, 0)),
    make_tuple(make_pair(-5, 0), make_pair(5, 5), make_pair(0, 5)),
    make_tuple(make_pair(-5, 5), make_pair(-5, -5), make_pair(-10, 0)),
    make_tuple(make_pair(-5, 5), make_pair(-5, 0), make_pair(-10, 5)),
    make_tuple(make_pair(-5, 5), make_pair(-5, 5), make_pair(-10, 10)),
    make_tuple(make_pair(-5, 5), make_pair(0, -5), make_pair(-5, 0)),
    make_tuple(make_pair(-5, 5), make_pair(0, 0), make_pair(-5, 5)),
    make_tuple(make_pair(-5, 5), make_pair(0, 5), make_pair(-5, 10)),
    make_tuple(make_pair(-5, 5), make_pair(5, -5), make_pair(0, 0)),
    make_tuple(make_pair(-5, 5), make_pair(5, 0), make_pair(0, 5)),
    make_tuple(make_pair(-5, 5), make_pair(5, 5), make_pair(0, 10)),
    make_tuple(make_pair(0, -5), make_pair(-5, -5), make_pair(-5, -10)),
    make_tuple(make_pair(0, -5), make_pair(-5, 0), make_pair(-5, -5)),
    make_tuple(make_pair(0, -5), make_pair(-5, 5), make_pair(-5, 0)),
    make_tuple(make_pair(0, -5), make_pair(0, -5), make_pair(0, -10)),
    make_tuple(make_pair(0, -5), make_pair(0, 0), make_pair(0, -5)),
    make_tuple(make_pair(0, -5), make_pair(0, 5), make_pair(0, 0)),
    make_tuple(make_pair(0, -5), make_pair(5, -5), make_pair(5, -10)),
    make_tuple(make_pair(0, -5), make_pair(5, 0), make_pair(5, -5)),
    make_tuple(make_pair(0, -5), make_pair(5, 5), make_pair(5, 0)),
    make_tuple(make_pair(0, 0), make_pair(-5, -5), make_pair(-5, -5)),
    make_tuple(make_pair(0, 0), make_pair(-5, 0), make_pair(-5, 0)),
    make_tuple(make_pair(0, 0), make_pair(-5, 5), make_pair(-5, 5)),
    make_tuple(make_pair(0, 0), make_pair(0, -5), make_pair(0, -5)),
    make_tuple(make_pair(0, 0), make_pair(0, 0), make_pair(0, 0)),
    make_tuple(make_pair(0, 0), make_pair(0, 5), make_pair(0, 5)),
    make_tuple(make_pair(0, 0), make_pair(5, -5), make_pair(5, -5)),
    make_tuple(make_pair(0, 0), make_pair(5, 0), make_pair(5, 0)),
    make_tuple(make_pair(0, 0), make_pair(5, 5), make_pair(5, 5)),
    make_tuple(make_pair(0, 5), make_pair(-5, -5), make_pair(-5, 0)),
    make_tuple(make_pair(0, 5), make_pair(-5, 0), make_pair(-5, 5)),
    make_tuple(make_pair(0, 5), make_pair(-5, 5), make_pair(-5, 10)),
    make_tuple(make_pair(0, 5), make_pair(0, -5), make_pair(0, 0)),
    make_tuple(make_pair(0, 5), make_pair(0, 0), make_pair(0, 5)),
    make_tuple(make_pair(0, 5), make_pair(0, 5), make_pair(0, 10)),
    make_tuple(make_pair(0, 5), make_pair(5, -5), make_pair(5, 0)),
    make_tuple(make_pair(0, 5), make_pair(5, 0), make_pair(5, 5)),
    make_tuple(make_pair(0, 5), make_pair(5, 5), make_pair(5, 10)),
    make_tuple(make_pair(5, -5), make_pair(-5, -5), make_pair(0, -10)),
    make_tuple(make_pair(5, -5), make_pair(-5, 0), make_pair(0, -5)),
    make_tuple(make_pair(5, -5), make_pair(-5, 5), make_pair(0, 0)),
    make_tuple(make_pair(5, -5), make_pair(0, -5), make_pair(5, -10)),
    make_tuple(make_pair(5, -5), make_pair(0, 0), make_pair(5, -5)),
    make_tuple(make_pair(5, -5), make_pair(0, 5), make_pair(5, 0)),
    make_tuple(make_pair(5, -5), make_pair(5, -5), make_pair(10, -10)),
    make_tuple(make_pair(5, -5), make_pair(5, 0), make_pair(10, -5)),
    make_tuple(make_pair(5, -5), make_pair(5, 5), make_pair(10, 0)),
    make_tuple(make_pair(5, 0), make_pair(-5, -5), make_pair(0, -5)),
    make_tuple(make_pair(5, 0), make_pair(-5, 0), make_pair(0, 0)),
    make_tuple(make_pair(5, 0), make_pair(-5, 5), make_pair(0, 5)),
    make_tuple(make_pair(5, 0), make_pair(0, -5), make_pair(5, -5)),
    make_tuple(make_pair(5, 0), make_pair(0, 0), make_pair(5, 0)),
    make_tuple(make_pair(5, 0), make_pair(0, 5), make_pair(5, 5)),
    make_tuple(make_pair(5, 0), make_pair(5, -5), make_pair(10, -5)),
    make_tuple(make_pair(5, 0), make_pair(5, 0), make_pair(10, 0)),
    make_tuple(make_pair(5, 0), make_pair(5, 5), make_pair(10, 5)),
    make_tuple(make_pair(5, 5), make_pair(-5, -5), make_pair(0, 0)),
    make_tuple(make_pair(5, 5), make_pair(-5, 0), make_pair(0, 5)),
    make_tuple(make_pair(5, 5), make_pair(-5, 5), make_pair(0, 10)),
    make_tuple(make_pair(5, 5), make_pair(0, -5), make_pair(5, 0)),
    make_tuple(make_pair(5, 5), make_pair(0, 0), make_pair(5, 5)),
    make_tuple(make_pair(5, 5), make_pair(0, 5), make_pair(5, 10)),
    make_tuple(make_pair(5, 5), make_pair(5, -5), make_pair(10, 0)),
    make_tuple(make_pair(5, 5), make_pair(5, 0), make_pair(10, 5)),
    make_tuple(make_pair(5, 5), make_pair(5, 5), make_pair(10, 10)),
};

INSTANTIATE_TEST_CASE_P(SumTest, PointTest, ::testing::ValuesIn(valueSum));
TEST_P(PointTest, InPlaceAddTest1) {
  ASSERT_NO_THROW({
    a += b;
    ASSERT_EQ(a, c);
  });
}
TEST_P(PointTest, InPlaceAddTest2) {
  ASSERT_NO_THROW({
    b += a;
    ASSERT_EQ(b, c);
  });
}
TEST_P(PointTest, AddTest1) {
  ASSERT_NO_THROW({
    iPoint2D d = a + b;
    ASSERT_EQ(d, c);
  });
}
TEST_P(PointTest, AddTest2) {
  ASSERT_NO_THROW({
    iPoint2D d = b + a;
    ASSERT_EQ(d, c);
  });
}

TEST_P(PointTest, InPlaceSubTest1) {
  ASSERT_NO_THROW({
    c -= a;
    ASSERT_EQ(c, b);
  });
}
TEST_P(PointTest, InPlaceSubTest2) {
  ASSERT_NO_THROW({
    c -= b;
    ASSERT_EQ(c, a);
  });
}
TEST_P(PointTest, SubTest1) {
  ASSERT_NO_THROW({
    iPoint2D d = c - a;
    ASSERT_EQ(d, b);
  });
}
TEST_P(PointTest, SubTest2) {
  ASSERT_NO_THROW({
    iPoint2D d = c - b;
    ASSERT_EQ(d, a);
  });
}

using areaType = tuple<IntPair, iPoint2D::area_type>;
class AreaTest : public ::testing::TestWithParam<areaType> {
protected:
  AreaTest() = default;
  virtual void SetUp() {
    auto param = GetParam();

    auto pair = std::tr1::get<0>(param);
    p = iPoint2D(pair.first, pair.second);

    a = std::tr1::get<1>(param);
  }

  iPoint2D p;
  iPoint2D::area_type a;
};

/*
#!/bin/bash
for i in -5 0 5;
do
  for j in -5 0 5;
  do
    k=$(($i*$j))
    if [[ $k -lt 0 ]]
    then
      k=$((-$k));
    fi;
    echo "make_tuple(make_pair($i, $j), $k),";
  done;
done;
*/
static const areaType valueArea[]{
    make_tuple(make_pair(-5, -5), 25),
    make_tuple(make_pair(-5, 0), 0),
    make_tuple(make_pair(-5, 5), 25),
    make_tuple(make_pair(0, -5), 0),
    make_tuple(make_pair(0, 0), 0),
    make_tuple(make_pair(0, 5), 0),
    make_tuple(make_pair(5, -5), 25),
    make_tuple(make_pair(5, 0), 0),
    make_tuple(make_pair(5, 5), 25),

    make_tuple(make_pair(minVal, 0), 0),
    make_tuple(make_pair(maxVal, 0), 0),
    make_tuple(make_pair(minVal, -1), absMinVal),
    make_tuple(make_pair(maxVal, -1), maxVal),
    make_tuple(make_pair(minVal, 1), absMinVal),
    make_tuple(make_pair(maxVal, 1), maxVal),

    make_tuple(make_pair(0, minVal), 0),
    make_tuple(make_pair(0, maxVal), 0),
    make_tuple(make_pair(-1, minVal), absMinVal),
    make_tuple(make_pair(-1, maxVal), maxVal),
    make_tuple(make_pair(1, minVal), absMinVal),
    make_tuple(make_pair(1, maxVal), maxVal),

    make_tuple(make_pair(minVal, minVal), minAreaVal),
    make_tuple(make_pair(minVal, maxVal), mixAreaVal),
    make_tuple(make_pair(maxVal, minVal), mixAreaVal),
    make_tuple(make_pair(maxVal, maxVal), maxAreaVal),

};
INSTANTIATE_TEST_CASE_P(AreaTest, AreaTest, ::testing::ValuesIn(valueArea));
TEST_P(AreaTest, AreaTest) {
  ASSERT_NO_THROW({ ASSERT_EQ(p.area(), a); });
}

using operatorsType =
    std::tr1::tuple<IntPair, IntPair, bool, bool, bool, bool, bool>;
class OperatorsTest : public ::testing::TestWithParam<operatorsType> {
protected:
  OperatorsTest() = default;
  virtual void SetUp() {
    auto p = GetParam();

    auto pair = std::tr1::get<0>(p);
    a = iPoint2D(pair.first, pair.second);

    pair = std::tr1::get<1>(p);
    b = iPoint2D(pair.first, pair.second);

    eq = std::tr1::get<2>(p);
    lt = std::tr1::get<3>(p);
    gt = std::tr1::get<4>(p);
    le = std::tr1::get<5>(p);
    ge = std::tr1::get<6>(p);
  }

  iPoint2D a;
  iPoint2D b;
  bool eq;
  bool lt;
  bool gt;
  bool le;
  bool ge;
};

/*
#!/bin/bash
for i in -1 0 1;
do
  for j in -1 0 1;
  do
    for k in -1 0 1;
    do
      for q in -1 0 1;
      do
        echo "make_tuple(make_pair($i, $j), make_pair($k, $q), "

        if [ $i -eq $k ] && [ $j -eq $q ]
        then
          echo "true, "
        else
          echo "false, "
        fi;

        if [ $i -lt $k ] && [ $j -lt $q ]
        then
          echo "true, "
        else
          echo "false, "
        fi;

        if [ $i -gt $k ] && [ $j -gt $q ]
        then
          echo "true, "
        else
          echo "false, "
        fi;

        if [ $i -le $k ] && [ $j -le $q ]
        then
          echo "true, "
        else
          echo "false, "
        fi;

        if [ $i -ge $k ] && [ $j -ge $q ]
        then
          echo "true"
        else
          echo "false"
        fi;

        echo "),";
      done;
    done;
  done;
done;
*/
static const operatorsType operatorsValues[]{
    make_tuple(make_pair(-1, -1), make_pair(-1, -1), true, false, false, true,
               true),
    make_tuple(make_pair(-1, -1), make_pair(-1, 0), false, false, false, true,
               false),
    make_tuple(make_pair(-1, -1), make_pair(-1, 1), false, false, false, true,
               false),
    make_tuple(make_pair(-1, -1), make_pair(0, -1), false, false, false, true,
               false),
    make_tuple(make_pair(-1, -1), make_pair(0, 0), false, true, false, true,
               false),
    make_tuple(make_pair(-1, -1), make_pair(0, 1), false, true, false, true,
               false),
    make_tuple(make_pair(-1, -1), make_pair(1, -1), false, false, false, true,
               false),
    make_tuple(make_pair(-1, -1), make_pair(1, 0), false, true, false, true,
               false),
    make_tuple(make_pair(-1, -1), make_pair(1, 1), false, true, false, true,
               false),
    make_tuple(make_pair(-1, 0), make_pair(-1, -1), false, false, false, false,
               true),
    make_tuple(make_pair(-1, 0), make_pair(-1, 0), true, false, false, true,
               true),
    make_tuple(make_pair(-1, 0), make_pair(-1, 1), false, false, false, true,
               false),
    make_tuple(make_pair(-1, 0), make_pair(0, -1), false, false, false, false,
               false),
    make_tuple(make_pair(-1, 0), make_pair(0, 0), false, false, false, true,
               false),
    make_tuple(make_pair(-1, 0), make_pair(0, 1), false, true, false, true,
               false),
    make_tuple(make_pair(-1, 0), make_pair(1, -1), false, false, false, false,
               false),
    make_tuple(make_pair(-1, 0), make_pair(1, 0), false, false, false, true,
               false),
    make_tuple(make_pair(-1, 0), make_pair(1, 1), false, true, false, true,
               false),
    make_tuple(make_pair(-1, 1), make_pair(-1, -1), false, false, false, false,
               true),
    make_tuple(make_pair(-1, 1), make_pair(-1, 0), false, false, false, false,
               true),
    make_tuple(make_pair(-1, 1), make_pair(-1, 1), true, false, false, true,
               true),
    make_tuple(make_pair(-1, 1), make_pair(0, -1), false, false, false, false,
               false),
    make_tuple(make_pair(-1, 1), make_pair(0, 0), false, false, false, false,
               false),
    make_tuple(make_pair(-1, 1), make_pair(0, 1), false, false, false, true,
               false),
    make_tuple(make_pair(-1, 1), make_pair(1, -1), false, false, false, false,
               false),
    make_tuple(make_pair(-1, 1), make_pair(1, 0), false, false, false, false,
               false),
    make_tuple(make_pair(-1, 1), make_pair(1, 1), false, false, false, true,
               false),
    make_tuple(make_pair(0, -1), make_pair(-1, -1), false, false, false, false,
               true),
    make_tuple(make_pair(0, -1), make_pair(-1, 0), false, false, false, false,
               false),
    make_tuple(make_pair(0, -1), make_pair(-1, 1), false, false, false, false,
               false),
    make_tuple(make_pair(0, -1), make_pair(0, -1), true, false, false, true,
               true),
    make_tuple(make_pair(0, -1), make_pair(0, 0), false, false, false, true,
               false),
    make_tuple(make_pair(0, -1), make_pair(0, 1), false, false, false, true,
               false),
    make_tuple(make_pair(0, -1), make_pair(1, -1), false, false, false, true,
               false),
    make_tuple(make_pair(0, -1), make_pair(1, 0), false, true, false, true,
               false),
    make_tuple(make_pair(0, -1), make_pair(1, 1), false, true, false, true,
               false),
    make_tuple(make_pair(0, 0), make_pair(-1, -1), false, false, true, false,
               true),
    make_tuple(make_pair(0, 0), make_pair(-1, 0), false, false, false, false,
               true),
    make_tuple(make_pair(0, 0), make_pair(-1, 1), false, false, false, false,
               false),
    make_tuple(make_pair(0, 0), make_pair(0, -1), false, false, false, false,
               true),
    make_tuple(make_pair(0, 0), make_pair(0, 0), true, false, false, true,
               true),
    make_tuple(make_pair(0, 0), make_pair(0, 1), false, false, false, true,
               false),
    make_tuple(make_pair(0, 0), make_pair(1, -1), false, false, false, false,
               false),
    make_tuple(make_pair(0, 0), make_pair(1, 0), false, false, false, true,
               false),
    make_tuple(make_pair(0, 0), make_pair(1, 1), false, true, false, true,
               false),
    make_tuple(make_pair(0, 1), make_pair(-1, -1), false, false, true, false,
               true),
    make_tuple(make_pair(0, 1), make_pair(-1, 0), false, false, true, false,
               true),
    make_tuple(make_pair(0, 1), make_pair(-1, 1), false, false, false, false,
               true),
    make_tuple(make_pair(0, 1), make_pair(0, -1), false, false, false, false,
               true),
    make_tuple(make_pair(0, 1), make_pair(0, 0), false, false, false, false,
               true),
    make_tuple(make_pair(0, 1), make_pair(0, 1), true, false, false, true,
               true),
    make_tuple(make_pair(0, 1), make_pair(1, -1), false, false, false, false,
               false),
    make_tuple(make_pair(0, 1), make_pair(1, 0), false, false, false, false,
               false),
    make_tuple(make_pair(0, 1), make_pair(1, 1), false, false, false, true,
               false),
    make_tuple(make_pair(1, -1), make_pair(-1, -1), false, false, false, false,
               true),
    make_tuple(make_pair(1, -1), make_pair(-1, 0), false, false, false, false,
               false),
    make_tuple(make_pair(1, -1), make_pair(-1, 1), false, false, false, false,
               false),
    make_tuple(make_pair(1, -1), make_pair(0, -1), false, false, false, false,
               true),
    make_tuple(make_pair(1, -1), make_pair(0, 0), false, false, false, false,
               false),
    make_tuple(make_pair(1, -1), make_pair(0, 1), false, false, false, false,
               false),
    make_tuple(make_pair(1, -1), make_pair(1, -1), true, false, false, true,
               true),
    make_tuple(make_pair(1, -1), make_pair(1, 0), false, false, false, true,
               false),
    make_tuple(make_pair(1, -1), make_pair(1, 1), false, false, false, true,
               false),
    make_tuple(make_pair(1, 0), make_pair(-1, -1), false, false, true, false,
               true),
    make_tuple(make_pair(1, 0), make_pair(-1, 0), false, false, false, false,
               true),
    make_tuple(make_pair(1, 0), make_pair(-1, 1), false, false, false, false,
               false),
    make_tuple(make_pair(1, 0), make_pair(0, -1), false, false, true, false,
               true),
    make_tuple(make_pair(1, 0), make_pair(0, 0), false, false, false, false,
               true),
    make_tuple(make_pair(1, 0), make_pair(0, 1), false, false, false, false,
               false),
    make_tuple(make_pair(1, 0), make_pair(1, -1), false, false, false, false,
               true),
    make_tuple(make_pair(1, 0), make_pair(1, 0), true, false, false, true,
               true),
    make_tuple(make_pair(1, 0), make_pair(1, 1), false, false, false, true,
               false),
    make_tuple(make_pair(1, 1), make_pair(-1, -1), false, false, true, false,
               true),
    make_tuple(make_pair(1, 1), make_pair(-1, 0), false, false, true, false,
               true),
    make_tuple(make_pair(1, 1), make_pair(-1, 1), false, false, false, false,
               true),
    make_tuple(make_pair(1, 1), make_pair(0, -1), false, false, true, false,
               true),
    make_tuple(make_pair(1, 1), make_pair(0, 0), false, false, true, false,
               true),
    make_tuple(make_pair(1, 1), make_pair(0, 1), false, false, false, false,
               true),
    make_tuple(make_pair(1, 1), make_pair(1, -1), false, false, false, false,
               true),
    make_tuple(make_pair(1, 1), make_pair(1, 0), false, false, false, false,
               true),
    make_tuple(make_pair(1, 1), make_pair(1, 1), true, false, false, true,
               true)};

INSTANTIATE_TEST_CASE_P(OperatorsTests, OperatorsTest,
                        ::testing::ValuesIn(operatorsValues));

TEST_P(OperatorsTest, OperatorEQTest) {
  ASSERT_NO_THROW({ ASSERT_EQ(a == b, eq); });
  ASSERT_NO_THROW({ ASSERT_EQ(b == a, eq); });
}
TEST_P(OperatorsTest, OperatorNETest) {
  ASSERT_NO_THROW({ ASSERT_EQ(a != b, !eq); });
  ASSERT_NO_THROW({ ASSERT_EQ(b != a, !eq); });
}

TEST_P(OperatorsTest, OperatorLTTest) {
  ASSERT_NO_THROW({ ASSERT_EQ(a < b, lt); });
  ASSERT_NO_THROW({ ASSERT_EQ(b > a, lt); });
}
TEST_P(OperatorsTest, OperatorGTest) {
  ASSERT_NO_THROW({ ASSERT_EQ(a > b, gt); });
  ASSERT_NO_THROW({ ASSERT_EQ(b < a, gt); });
}

TEST_P(OperatorsTest, OperatorLETest) {
  ASSERT_NO_THROW({ ASSERT_EQ(a <= b, le); });
  ASSERT_NO_THROW({ ASSERT_EQ(b >= a, le); });
}
TEST_P(OperatorsTest, OperatorGEest) {
  ASSERT_NO_THROW({ ASSERT_EQ(a >= b, ge); });
  ASSERT_NO_THROW({ ASSERT_EQ(b <= a, ge); });
}

TEST_P(OperatorsTest, OperatorsTest) {
  ASSERT_NO_THROW({ ASSERT_EQ(a.isThisInside(b), le); });
}

/*
#!/bin/bash
for i in -5 0 5;
do
  for j in -5 0 5;
  do
    for k in -5 0 5;
    do
      for q in -5 0 5;
      do
        echo "make_tuple(make_pair($i, $j), make_pair($k, $q),
make_pair($(($i<=$k?$i:$k)), $(($j<=$q?$j:$q)))),";
      done;
    done;
  done;
done;
*/
static const Six smallestValues[]{
    make_tuple(make_pair(-5, -5), make_pair(-5, -5), make_pair(-5, -5)),
    make_tuple(make_pair(-5, -5), make_pair(-5, 0), make_pair(-5, -5)),
    make_tuple(make_pair(-5, -5), make_pair(-5, 5), make_pair(-5, -5)),
    make_tuple(make_pair(-5, -5), make_pair(0, -5), make_pair(-5, -5)),
    make_tuple(make_pair(-5, -5), make_pair(0, 0), make_pair(-5, -5)),
    make_tuple(make_pair(-5, -5), make_pair(0, 5), make_pair(-5, -5)),
    make_tuple(make_pair(-5, -5), make_pair(5, -5), make_pair(-5, -5)),
    make_tuple(make_pair(-5, -5), make_pair(5, 0), make_pair(-5, -5)),
    make_tuple(make_pair(-5, -5), make_pair(5, 5), make_pair(-5, -5)),
    make_tuple(make_pair(-5, 0), make_pair(-5, -5), make_pair(-5, -5)),
    make_tuple(make_pair(-5, 0), make_pair(-5, 0), make_pair(-5, 0)),
    make_tuple(make_pair(-5, 0), make_pair(-5, 5), make_pair(-5, 0)),
    make_tuple(make_pair(-5, 0), make_pair(0, -5), make_pair(-5, -5)),
    make_tuple(make_pair(-5, 0), make_pair(0, 0), make_pair(-5, 0)),
    make_tuple(make_pair(-5, 0), make_pair(0, 5), make_pair(-5, 0)),
    make_tuple(make_pair(-5, 0), make_pair(5, -5), make_pair(-5, -5)),
    make_tuple(make_pair(-5, 0), make_pair(5, 0), make_pair(-5, 0)),
    make_tuple(make_pair(-5, 0), make_pair(5, 5), make_pair(-5, 0)),
    make_tuple(make_pair(-5, 5), make_pair(-5, -5), make_pair(-5, -5)),
    make_tuple(make_pair(-5, 5), make_pair(-5, 0), make_pair(-5, 0)),
    make_tuple(make_pair(-5, 5), make_pair(-5, 5), make_pair(-5, 5)),
    make_tuple(make_pair(-5, 5), make_pair(0, -5), make_pair(-5, -5)),
    make_tuple(make_pair(-5, 5), make_pair(0, 0), make_pair(-5, 0)),
    make_tuple(make_pair(-5, 5), make_pair(0, 5), make_pair(-5, 5)),
    make_tuple(make_pair(-5, 5), make_pair(5, -5), make_pair(-5, -5)),
    make_tuple(make_pair(-5, 5), make_pair(5, 0), make_pair(-5, 0)),
    make_tuple(make_pair(-5, 5), make_pair(5, 5), make_pair(-5, 5)),
    make_tuple(make_pair(0, -5), make_pair(-5, -5), make_pair(-5, -5)),
    make_tuple(make_pair(0, -5), make_pair(-5, 0), make_pair(-5, -5)),
    make_tuple(make_pair(0, -5), make_pair(-5, 5), make_pair(-5, -5)),
    make_tuple(make_pair(0, -5), make_pair(0, -5), make_pair(0, -5)),
    make_tuple(make_pair(0, -5), make_pair(0, 0), make_pair(0, -5)),
    make_tuple(make_pair(0, -5), make_pair(0, 5), make_pair(0, -5)),
    make_tuple(make_pair(0, -5), make_pair(5, -5), make_pair(0, -5)),
    make_tuple(make_pair(0, -5), make_pair(5, 0), make_pair(0, -5)),
    make_tuple(make_pair(0, -5), make_pair(5, 5), make_pair(0, -5)),
    make_tuple(make_pair(0, 0), make_pair(-5, -5), make_pair(-5, -5)),
    make_tuple(make_pair(0, 0), make_pair(-5, 0), make_pair(-5, 0)),
    make_tuple(make_pair(0, 0), make_pair(-5, 5), make_pair(-5, 0)),
    make_tuple(make_pair(0, 0), make_pair(0, -5), make_pair(0, -5)),
    make_tuple(make_pair(0, 0), make_pair(0, 0), make_pair(0, 0)),
    make_tuple(make_pair(0, 0), make_pair(0, 5), make_pair(0, 0)),
    make_tuple(make_pair(0, 0), make_pair(5, -5), make_pair(0, -5)),
    make_tuple(make_pair(0, 0), make_pair(5, 0), make_pair(0, 0)),
    make_tuple(make_pair(0, 0), make_pair(5, 5), make_pair(0, 0)),
    make_tuple(make_pair(0, 5), make_pair(-5, -5), make_pair(-5, -5)),
    make_tuple(make_pair(0, 5), make_pair(-5, 0), make_pair(-5, 0)),
    make_tuple(make_pair(0, 5), make_pair(-5, 5), make_pair(-5, 5)),
    make_tuple(make_pair(0, 5), make_pair(0, -5), make_pair(0, -5)),
    make_tuple(make_pair(0, 5), make_pair(0, 0), make_pair(0, 0)),
    make_tuple(make_pair(0, 5), make_pair(0, 5), make_pair(0, 5)),
    make_tuple(make_pair(0, 5), make_pair(5, -5), make_pair(0, -5)),
    make_tuple(make_pair(0, 5), make_pair(5, 0), make_pair(0, 0)),
    make_tuple(make_pair(0, 5), make_pair(5, 5), make_pair(0, 5)),
    make_tuple(make_pair(5, -5), make_pair(-5, -5), make_pair(-5, -5)),
    make_tuple(make_pair(5, -5), make_pair(-5, 0), make_pair(-5, -5)),
    make_tuple(make_pair(5, -5), make_pair(-5, 5), make_pair(-5, -5)),
    make_tuple(make_pair(5, -5), make_pair(0, -5), make_pair(0, -5)),
    make_tuple(make_pair(5, -5), make_pair(0, 0), make_pair(0, -5)),
    make_tuple(make_pair(5, -5), make_pair(0, 5), make_pair(0, -5)),
    make_tuple(make_pair(5, -5), make_pair(5, -5), make_pair(5, -5)),
    make_tuple(make_pair(5, -5), make_pair(5, 0), make_pair(5, -5)),
    make_tuple(make_pair(5, -5), make_pair(5, 5), make_pair(5, -5)),
    make_tuple(make_pair(5, 0), make_pair(-5, -5), make_pair(-5, -5)),
    make_tuple(make_pair(5, 0), make_pair(-5, 0), make_pair(-5, 0)),
    make_tuple(make_pair(5, 0), make_pair(-5, 5), make_pair(-5, 0)),
    make_tuple(make_pair(5, 0), make_pair(0, -5), make_pair(0, -5)),
    make_tuple(make_pair(5, 0), make_pair(0, 0), make_pair(0, 0)),
    make_tuple(make_pair(5, 0), make_pair(0, 5), make_pair(0, 0)),
    make_tuple(make_pair(5, 0), make_pair(5, -5), make_pair(5, -5)),
    make_tuple(make_pair(5, 0), make_pair(5, 0), make_pair(5, 0)),
    make_tuple(make_pair(5, 0), make_pair(5, 5), make_pair(5, 0)),
    make_tuple(make_pair(5, 5), make_pair(-5, -5), make_pair(-5, -5)),
    make_tuple(make_pair(5, 5), make_pair(-5, 0), make_pair(-5, 0)),
    make_tuple(make_pair(5, 5), make_pair(-5, 5), make_pair(-5, 5)),
    make_tuple(make_pair(5, 5), make_pair(0, -5), make_pair(0, -5)),
    make_tuple(make_pair(5, 5), make_pair(0, 0), make_pair(0, 0)),
    make_tuple(make_pair(5, 5), make_pair(0, 5), make_pair(0, 5)),
    make_tuple(make_pair(5, 5), make_pair(5, -5), make_pair(5, -5)),
    make_tuple(make_pair(5, 5), make_pair(5, 0), make_pair(5, 0)),
    make_tuple(make_pair(5, 5), make_pair(5, 5), make_pair(5, 5)),
};

class SmallestTest : public PointTest {};

INSTANTIATE_TEST_CASE_P(GetSmallestTest, SmallestTest,
                        ::testing::ValuesIn(smallestValues));
TEST_P(SmallestTest, GetSmallestTest) {
  ASSERT_NO_THROW({
    ASSERT_EQ(a.getSmallest(b), c);
    ASSERT_EQ(a.getSmallest(c), c);
    ASSERT_EQ(b.getSmallest(a), c);
    ASSERT_EQ(b.getSmallest(c), c);
    ASSERT_EQ(c.getSmallest(a), c);
    ASSERT_EQ(c.getSmallest(b), c);
    ASSERT_EQ(c.getSmallest(c), c);
  });
}

} // namespace rawspeed_test
