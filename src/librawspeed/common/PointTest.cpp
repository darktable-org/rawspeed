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
#include <ostream>        // for operator<<, basic_ostream::operator<<, ost...
#include <utility>        // for make_pair, pair, move

using namespace std;
using namespace RawSpeed;

::std::ostream& operator<<(::std::ostream& os, const iPoint2D p) {
  return os << "(" << p.x << ", " << p.y << ")";
}

TEST(PointTest, Constructor) {
  ASSERT_NO_THROW({
    iPoint2D a;
    ASSERT_EQ(a.x, 0);
    ASSERT_EQ(a.y, 0);
  });
  ASSERT_NO_THROW({
    iPoint2D a(-10, 15);
    ASSERT_EQ(a.x, -10);
    ASSERT_EQ(a.y, 15);
  });
  ASSERT_NO_THROW({
    const iPoint2D a(-10, 15);
    iPoint2D b(a);
    ASSERT_EQ(b.x, -10);
    ASSERT_EQ(b.y, 15);
  });
}

TEST(PointTest, AssignmentConstructor) {
  ASSERT_NO_THROW({
    const iPoint2D a(-10, 15);
    iPoint2D b(666, 777);
    b = a;
    ASSERT_EQ(b.x, -10);
    ASSERT_EQ(b.y, 15);
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

using areaType = tuple<IntPair, int>;
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
  int a;
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
static const areaType valueMul[]{
    make_tuple(make_pair(-5, -5), 25), make_tuple(make_pair(-5, 0), 0),
    make_tuple(make_pair(-5, 5), 25),  make_tuple(make_pair(0, -5), 0),
    make_tuple(make_pair(0, 0), 0),    make_tuple(make_pair(0, 5), 0),
    make_tuple(make_pair(5, -5), 25),  make_tuple(make_pair(5, 0), 0),
    make_tuple(make_pair(5, 5), 25),

};
INSTANTIATE_TEST_CASE_P(SubTest, AreaTest, ::testing::ValuesIn(valueMul));
TEST_P(AreaTest, AreaTest) {
  ASSERT_NO_THROW({ ASSERT_EQ(p.area(), a); });
}

using isThisInsideType = std::tr1::tuple<IntPair, IntPair, bool>;
class IsThisInsideTest : public ::testing::TestWithParam<isThisInsideType> {
protected:
  IsThisInsideTest() = default;
  virtual void SetUp() {
    auto p = GetParam();

    auto pair = std::tr1::get<0>(p);
    a = iPoint2D(pair.first, pair.second);

    pair = std::tr1::get<1>(p);
    b = iPoint2D(pair.first, pair.second);

    res = std::tr1::get<2>(p);
  }

  iPoint2D a;
  iPoint2D b;
  bool res;
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
        if [ $i -le $k ] && [ $j -le $q ]
        then
          echo "make_tuple(make_pair($i, $j), make_pair($k, $q), true),";
        else
          echo "make_tuple(make_pair($i, $j), make_pair($k, $q), false),";
        fi;
      done;
    done;
  done;
done;
*/
static const isThisInsideType isThisInsideValues[]{
    make_tuple(make_pair(-1, -1), make_pair(-1, -1), true),
    make_tuple(make_pair(-1, -1), make_pair(-1, 0), true),
    make_tuple(make_pair(-1, -1), make_pair(-1, 1), true),
    make_tuple(make_pair(-1, -1), make_pair(0, -1), true),
    make_tuple(make_pair(-1, -1), make_pair(0, 0), true),
    make_tuple(make_pair(-1, -1), make_pair(0, 1), true),
    make_tuple(make_pair(-1, -1), make_pair(1, -1), true),
    make_tuple(make_pair(-1, -1), make_pair(1, 0), true),
    make_tuple(make_pair(-1, -1), make_pair(1, 1), true),
    make_tuple(make_pair(-1, 0), make_pair(-1, -1), false),
    make_tuple(make_pair(-1, 0), make_pair(-1, 0), true),
    make_tuple(make_pair(-1, 0), make_pair(-1, 1), true),
    make_tuple(make_pair(-1, 0), make_pair(0, -1), false),
    make_tuple(make_pair(-1, 0), make_pair(0, 0), true),
    make_tuple(make_pair(-1, 0), make_pair(0, 1), true),
    make_tuple(make_pair(-1, 0), make_pair(1, -1), false),
    make_tuple(make_pair(-1, 0), make_pair(1, 0), true),
    make_tuple(make_pair(-1, 0), make_pair(1, 1), true),
    make_tuple(make_pair(-1, 1), make_pair(-1, -1), false),
    make_tuple(make_pair(-1, 1), make_pair(-1, 0), false),
    make_tuple(make_pair(-1, 1), make_pair(-1, 1), true),
    make_tuple(make_pair(-1, 1), make_pair(0, -1), false),
    make_tuple(make_pair(-1, 1), make_pair(0, 0), false),
    make_tuple(make_pair(-1, 1), make_pair(0, 1), true),
    make_tuple(make_pair(-1, 1), make_pair(1, -1), false),
    make_tuple(make_pair(-1, 1), make_pair(1, 0), false),
    make_tuple(make_pair(-1, 1), make_pair(1, 1), true),
    make_tuple(make_pair(0, -1), make_pair(-1, -1), false),
    make_tuple(make_pair(0, -1), make_pair(-1, 0), false),
    make_tuple(make_pair(0, -1), make_pair(-1, 1), false),
    make_tuple(make_pair(0, -1), make_pair(0, -1), true),
    make_tuple(make_pair(0, -1), make_pair(0, 0), true),
    make_tuple(make_pair(0, -1), make_pair(0, 1), true),
    make_tuple(make_pair(0, -1), make_pair(1, -1), true),
    make_tuple(make_pair(0, -1), make_pair(1, 0), true),
    make_tuple(make_pair(0, -1), make_pair(1, 1), true),
    make_tuple(make_pair(0, 0), make_pair(-1, -1), false),
    make_tuple(make_pair(0, 0), make_pair(-1, 0), false),
    make_tuple(make_pair(0, 0), make_pair(-1, 1), false),
    make_tuple(make_pair(0, 0), make_pair(0, -1), false),
    make_tuple(make_pair(0, 0), make_pair(0, 0), true),
    make_tuple(make_pair(0, 0), make_pair(0, 1), true),
    make_tuple(make_pair(0, 0), make_pair(1, -1), false),
    make_tuple(make_pair(0, 0), make_pair(1, 0), true),
    make_tuple(make_pair(0, 0), make_pair(1, 1), true),
    make_tuple(make_pair(0, 1), make_pair(-1, -1), false),
    make_tuple(make_pair(0, 1), make_pair(-1, 0), false),
    make_tuple(make_pair(0, 1), make_pair(-1, 1), false),
    make_tuple(make_pair(0, 1), make_pair(0, -1), false),
    make_tuple(make_pair(0, 1), make_pair(0, 0), false),
    make_tuple(make_pair(0, 1), make_pair(0, 1), true),
    make_tuple(make_pair(0, 1), make_pair(1, -1), false),
    make_tuple(make_pair(0, 1), make_pair(1, 0), false),
    make_tuple(make_pair(0, 1), make_pair(1, 1), true),
    make_tuple(make_pair(1, -1), make_pair(-1, -1), false),
    make_tuple(make_pair(1, -1), make_pair(-1, 0), false),
    make_tuple(make_pair(1, -1), make_pair(-1, 1), false),
    make_tuple(make_pair(1, -1), make_pair(0, -1), false),
    make_tuple(make_pair(1, -1), make_pair(0, 0), false),
    make_tuple(make_pair(1, -1), make_pair(0, 1), false),
    make_tuple(make_pair(1, -1), make_pair(1, -1), true),
    make_tuple(make_pair(1, -1), make_pair(1, 0), true),
    make_tuple(make_pair(1, -1), make_pair(1, 1), true),
    make_tuple(make_pair(1, 0), make_pair(-1, -1), false),
    make_tuple(make_pair(1, 0), make_pair(-1, 0), false),
    make_tuple(make_pair(1, 0), make_pair(-1, 1), false),
    make_tuple(make_pair(1, 0), make_pair(0, -1), false),
    make_tuple(make_pair(1, 0), make_pair(0, 0), false),
    make_tuple(make_pair(1, 0), make_pair(0, 1), false),
    make_tuple(make_pair(1, 0), make_pair(1, -1), false),
    make_tuple(make_pair(1, 0), make_pair(1, 0), true),
    make_tuple(make_pair(1, 0), make_pair(1, 1), true),
    make_tuple(make_pair(1, 1), make_pair(-1, -1), false),
    make_tuple(make_pair(1, 1), make_pair(-1, 0), false),
    make_tuple(make_pair(1, 1), make_pair(-1, 1), false),
    make_tuple(make_pair(1, 1), make_pair(0, -1), false),
    make_tuple(make_pair(1, 1), make_pair(0, 0), false),
    make_tuple(make_pair(1, 1), make_pair(0, 1), false),
    make_tuple(make_pair(1, 1), make_pair(1, -1), false),
    make_tuple(make_pair(1, 1), make_pair(1, 0), false),
    make_tuple(make_pair(1, 1), make_pair(1, 1), true),
};

INSTANTIATE_TEST_CASE_P(IsThisInsideTest, IsThisInsideTest,
                        ::testing::ValuesIn(isThisInsideValues));
TEST_P(IsThisInsideTest, IsThisInsideTest) {
  ASSERT_NO_THROW({ ASSERT_EQ(a.isThisInside(b), res); });
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
