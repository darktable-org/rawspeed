/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2016 Roman Lebedev

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include "common/Common.h"             // for uint32
#include "common/Point.h"              // for iPoint2D
#include "metadata/ColorFilterArray.h" // for ColorFilterArray, (anonymous)
#include <gtest/gtest.h>               // for AssertionResult, IsNullLitera...
#include <memory>                      // for unique_ptr
#include <string>                      // for string

using namespace std;
using namespace RawSpeed;

using Bayer2x2 = std::tr1::tuple<int, int, int, int>;

static const iPoint2D square(2, 2);

TEST(ColorFilterArrayTestBasic, Constructor) {
  ASSERT_NO_THROW({
    ColorFilterArray cfa(square);
    ASSERT_EQ(cfa.getSize().area(), square.area());
  });
}

TEST(ColorFilterArrayTestBasic, SetSize) {
  ASSERT_NO_THROW({
    ColorFilterArray cfa;
    cfa.setSize(square);
    ASSERT_EQ(cfa.getSize().area(), square.area());
  });

  ASSERT_NO_THROW({
    ColorFilterArray cfa(iPoint2D(1, 1));
    cfa.setSize(square);
    ASSERT_EQ(cfa.getSize().area(), square.area());
  });
}

TEST(ColorFilterArrayTestBasic, ToDcraw) {
  ASSERT_NO_THROW({
    ColorFilterArray cfa(iPoint2D(6, 6));
    ASSERT_EQ(cfa.getDcrawFilter(), 9); // xtrans magic
  });
}

class ColorFilterArrayTest : public ::testing::TestWithParam<Bayer2x2> {
protected:
  ColorFilterArrayTest() = default;
  virtual void SetUp() { param = GetParam(); }

  Bayer2x2 param;
};

INSTANTIATE_TEST_CASE_P(RGBG2, ColorFilterArrayTest,
                        testing::Combine(testing::Range(0, 4),
                                         testing::Range(0, 4),
                                         testing::Range(0, 4),
                                         testing::Range(0, 4)));

static void setHelper(ColorFilterArray *cfa, Bayer2x2 param) {
  cfa->setCFA(square, std::tr1::get<0>(param), std::tr1::get<1>(param),
              std::tr1::get<2>(param), std::tr1::get<3>(param));
}

static void check(ColorFilterArray *cfa, Bayer2x2 param) {
  ASSERT_EQ(cfa->getColorAt(0, 0), std::tr1::get<0>(param));
  ASSERT_EQ(cfa->getColorAt(1, 0), std::tr1::get<1>(param));
  ASSERT_EQ(cfa->getColorAt(0, 1), std::tr1::get<2>(param));
  ASSERT_EQ(cfa->getColorAt(1, 1), std::tr1::get<3>(param));
}

TEST_P(ColorFilterArrayTest, Constructor) {
  ASSERT_NO_THROW({
    ColorFilterArray cfa;
    setHelper(&cfa, param);
    check(&cfa, param);
  });
}

TEST_P(ColorFilterArrayTest, AssignmentConstructor) {
  ASSERT_NO_THROW({
    ColorFilterArray cfaOrig;
    setHelper(&cfaOrig, param);
    check(&cfaOrig, param);

    ColorFilterArray cfa(cfaOrig);
    check(&cfa, param);
  });

  ASSERT_NO_THROW({
    ColorFilterArray cfaOrig;
    setHelper(&cfaOrig, param);
    check(&cfaOrig, param);

    ColorFilterArray cfa;
    cfa = cfaOrig;
    check(&cfa, param);
  });
}

TEST_P(ColorFilterArrayTest, ToDcraw) {
  ASSERT_NO_THROW({
    ColorFilterArray cfa;
    setHelper(&cfa, param);
    cfa.getDcrawFilter();
  });
}

TEST_P(ColorFilterArrayTest, DcrawFilterShift1) {
  uint32 bggr = 0x16161616;
  uint32 grbg = 0x61616161;
  uint32 gbrg = 0x49494949;
  uint32 rggb = 0x94949494;
  ASSERT_NO_THROW({
    ASSERT_EQ(ColorFilterArray::shiftDcrawFilter(rggb, 0, 0), rggb);
    ASSERT_EQ(ColorFilterArray::shiftDcrawFilter(rggb, 1, 0), grbg);
    ASSERT_EQ(ColorFilterArray::shiftDcrawFilter(rggb, 0, 1), gbrg);
    ASSERT_EQ(ColorFilterArray::shiftDcrawFilter(rggb, 1, 1), bggr);
    ASSERT_EQ(ColorFilterArray::shiftDcrawFilter(rggb, 0, 2), rggb);
  });
}

static iPoint2D shifts[] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}, {0, 2}};

TEST_P(ColorFilterArrayTest, DcrawFilterShift2) {
  ASSERT_NO_THROW({
    ColorFilterArray cfaOrig;
    setHelper(&cfaOrig, param);
    uint32 fo = cfaOrig.getDcrawFilter();

    for (auto s : shifts) {
      ColorFilterArray cfa = cfaOrig;
      cfa.shiftLeft(s.x);
      cfa.shiftDown(s.y);
      uint32 f = cfa.getDcrawFilter();
      ASSERT_EQ(f, ColorFilterArray::shiftDcrawFilter(fo, s.x, s.y));
    }
  });
}

TEST_P(ColorFilterArrayTest, AsString) {
  ASSERT_NO_THROW({
    ColorFilterArray cfa;
    setHelper(&cfa, param);
    string dsc = cfa.asString();

    ASSERT_GT(dsc.size(), 15);
    ASSERT_LE(dsc.size(), 32);
  });
}
