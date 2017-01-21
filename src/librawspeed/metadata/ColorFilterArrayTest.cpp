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

#include "metadata/ColorFilterArray.h" // for ColorFilterArray
#include "common/Point.h"            // for ColorFilterArray
#include <gmock/gmock.h> // for IsNullLiteralHelper, AssertionResult, gtest_ar
#include <gtest/gtest.h> // for IsNullLiteralHelper, AssertionResult, gtest_ar
#include <memory>        // for unique_ptr

using namespace std;
using namespace RawSpeed;

typedef std::tr1::tuple<int, int, int, int> Bayer2x2;

static const iPoint2D square(2, 2);

TEST(ColorFilterArrayTestBasic, Constructor) {
  ASSERT_NO_THROW({
    ColorFilterArray cfa(square);
    ASSERT_EQ(cfa.size.area(), square.area());
  });

  ASSERT_NO_THROW({
    ColorFilterArray cfa((uint32)0);
    ASSERT_EQ(cfa.size.area(), 8 * 2);
  });

  ASSERT_NO_THROW({
    unique_ptr<ColorFilterArray> cfa(new ColorFilterArray(square));
    ASSERT_EQ(cfa->size.area(), square.area());
  });

  ASSERT_NO_THROW({
    unique_ptr<ColorFilterArray> cfa(new ColorFilterArray((uint32)0));
    ASSERT_EQ(cfa->size.area(), 8 * 2);
  });
}

TEST(ColorFilterArrayTestBasic, SetSize) {
  ASSERT_NO_THROW({
    ColorFilterArray cfa;
    cfa.setSize(square);
    ASSERT_EQ(cfa.size.area(), square.area());
  });

  ASSERT_NO_THROW({
    unique_ptr<ColorFilterArray> cfa(new ColorFilterArray);
    cfa->setSize(square);
    ASSERT_EQ(cfa->size.area(), square.area());
  });

  ASSERT_NO_THROW({
    ColorFilterArray cfa(iPoint2D(1, 1));
    cfa.setSize(square);
    ASSERT_EQ(cfa.size.area(), square.area());
  });

  ASSERT_NO_THROW({
    unique_ptr<ColorFilterArray> cfa(new ColorFilterArray(iPoint2D(1, 1)));
    cfa->setSize(square);
    ASSERT_EQ(cfa->size.area(), square.area());
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
  ColorFilterArrayTest() {}
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

TEST_P(ColorFilterArrayTest, ToDcrawAndBack) {
  ASSERT_NO_THROW({
    ColorFilterArray cfaOrig;
    setHelper(&cfaOrig, param);

    uint32 filters = cfaOrig.getDcrawFilter();

    ColorFilterArray cfa(filters);
    check(&cfa, param); // so it should be a NOP
  });
}

TEST_P(ColorFilterArrayTest, AsString) {
  ASSERT_NO_THROW({
    ColorFilterArray cfa;
    setHelper(&cfa, param);
    string dsc = cfa.asString();

    ASSERT_GT(dsc.size(), 15);
    ASSERT_LE(dsc.size(), 28);
  });
}
