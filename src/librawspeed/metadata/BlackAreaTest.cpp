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

#include "metadata/BlackArea.h"   // for BlackArea
#include <gtest/gtest.h> // for IsNullLiteralHelper, AssertionResult, gtest_ar

using namespace std;
using namespace RawSpeed;

class BlackAreaTest
    : public ::testing::TestWithParam<std::tr1::tuple<int, int, bool>> {
protected:
  BlackAreaTest() = default;
  virtual void SetUp() override {
    offset = std::tr1::get<0>(GetParam());
    size = std::tr1::get<1>(GetParam());
    isVertical = std::tr1::get<2>(GetParam());
  }

  void checkHelper(const BlackArea& a) {
    ASSERT_EQ(a.offset, offset);
    ASSERT_EQ(a.size, size);
    ASSERT_EQ(a.isVertical, isVertical);
  }

  int offset{0};          // Offset in bayer pixels.
  int size{0};            // Size in bayer pixels.
  bool isVertical{false}; // Otherwise horizontal
};

INSTANTIATE_TEST_CASE_P(BlackAreas, BlackAreaTest,
                        testing::Combine(testing::Range(0, 1000, 250), // offset
                                         testing::Range(0, 1000, 250), // size
                                         testing::Bool() // isVertical
                                         ));

TEST_P(BlackAreaTest, Constructor) {
  ASSERT_NO_THROW({ BlackArea Area(offset, size, isVertical); });
}

TEST_P(BlackAreaTest, Getters) {
  {
    const BlackArea Area(offset, size, isVertical);

    checkHelper(Area);
  }
}
