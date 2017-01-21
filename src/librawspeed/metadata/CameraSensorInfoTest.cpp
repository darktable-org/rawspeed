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

#include "metadata/CameraSensorInfo.h" // for CameraSensorInfo
#include <algorithm>          // for generate
#include <cstdlib>            // for rand, srand
#include <gmock/gmock.h>      // for get, InitGoogleMock, tuple
#include <gtest/gtest.h>      // for IsNullLiteralHelper, AssertionResult
#include <iostream>           // for operator<<, basic_ostream::operator<<
#include <limits>             // for numeric_limits
#include <memory>             // for unique_ptr
#include <string>             // for string
#include <vector>             // for vector, allocator

using namespace std;
using namespace RawSpeed;

std::vector<int> ISOList(6);

int main(int argc, char **argv) {
  ISOList.push_back(0);

  int n = {25};
  std::generate(ISOList.begin() + 1, ISOList.end(), [&n] { return n *= 4; });

  std::srand(2016122923);

  testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}

class CameraSensorInfoTestDumb
    : public ::testing::TestWithParam<std::tr1::tuple<int, int>> {
protected:
  CameraSensorInfoTestDumb()
      : mBlackLevel(std::rand()), // NOLINT do not need crypto-level randomness
        mWhiteLevel(std::rand()), // NOLINT do not need crypto-level randomness
        mMinIso(-1), mMaxIso(-1),
        mBlackLevelSeparate({
            std::rand(), // NOLINT do not need crypto-level randomness
            std::rand(), // NOLINT do not need crypto-level randomness
            std::rand(), // NOLINT do not need crypto-level randomness
            std::rand()  // NOLINT do not need crypto-level randomness
        }) {}
  virtual void SetUp() {
    mMinIso = std::tr1::get<0>(GetParam());
    mMaxIso = std::tr1::get<1>(GetParam());
  }

  void checkHelper(const CameraSensorInfo &csi) {
    ASSERT_EQ(csi.mBlackLevel, mBlackLevel);
    ASSERT_EQ(csi.mWhiteLevel, mWhiteLevel);
    ASSERT_EQ(csi.mMinIso, mMinIso);
    ASSERT_EQ(csi.mMaxIso, mMaxIso);
    ASSERT_EQ(csi.mBlackLevelSeparate, mBlackLevelSeparate);
  }

  void checkHelper(const CameraSensorInfo &a, const CameraSensorInfo &b) {
    ASSERT_EQ(a.mBlackLevel, b.mBlackLevel);
    ASSERT_EQ(a.mWhiteLevel, b.mWhiteLevel);
    ASSERT_EQ(a.mMinIso, b.mMinIso);
    ASSERT_EQ(a.mMaxIso, b.mMaxIso);
    ASSERT_EQ(a.mBlackLevelSeparate, b.mBlackLevelSeparate);
  }

  int mBlackLevel;
  int mWhiteLevel;
  int mMinIso;
  int mMaxIso;
  std::vector<int> mBlackLevelSeparate;
};

INSTANTIATE_TEST_CASE_P(MinMax, CameraSensorInfoTestDumb,
                        testing::Combine(testing::ValuesIn(ISOList), // min iso
                                         testing::ValuesIn(ISOList)  // max iso
                                         ));

TEST_P(CameraSensorInfoTestDumb, Constructor) {
  ASSERT_NO_THROW({
    CameraSensorInfo Info(mBlackLevel, mWhiteLevel, mMinIso, mMaxIso,
                          mBlackLevelSeparate);
  });

  ASSERT_NO_THROW({
    unique_ptr<CameraSensorInfo> Info(new CameraSensorInfo(
        mBlackLevel, mWhiteLevel, mMinIso, mMaxIso, mBlackLevelSeparate));
  });
}

TEST_P(CameraSensorInfoTestDumb, Getters) {
  {
    const CameraSensorInfo Info(mBlackLevel, mWhiteLevel, mMinIso, mMaxIso,
                                mBlackLevelSeparate);

    checkHelper(Info);
  }

  {
    const unique_ptr<const CameraSensorInfo> Info(new CameraSensorInfo(
        mBlackLevel, mWhiteLevel, mMinIso, mMaxIso, mBlackLevelSeparate));

    checkHelper(*Info);
  }
}

TEST_P(CameraSensorInfoTestDumb, AssignmentConstructor) {
  ASSERT_NO_THROW({
    const CameraSensorInfo InfoOrig(mBlackLevel, mWhiteLevel, mMinIso, mMaxIso,
                                    mBlackLevelSeparate);
    CameraSensorInfo Info(InfoOrig); // NOLINT trying to test the copy
  });

  ASSERT_NO_THROW({
    const unique_ptr<const CameraSensorInfo> InfoOrig(new CameraSensorInfo(
        mBlackLevel, mWhiteLevel, mMinIso, mMaxIso, mBlackLevelSeparate));
    unique_ptr<CameraSensorInfo> Info(new CameraSensorInfo(*InfoOrig));
  });

  ASSERT_NO_THROW({
    const CameraSensorInfo InfoOrig(mBlackLevel, mWhiteLevel, mMinIso, mMaxIso,
                                    mBlackLevelSeparate);
    unique_ptr<CameraSensorInfo> Info(new CameraSensorInfo(InfoOrig));
  });

  ASSERT_NO_THROW({
    const unique_ptr<const CameraSensorInfo> InfoOrig(new CameraSensorInfo(
        mBlackLevel, mWhiteLevel, mMinIso, mMaxIso, mBlackLevelSeparate));
    CameraSensorInfo Info(*InfoOrig);
  });
}

TEST_P(CameraSensorInfoTestDumb, AssignmentConstructorGetters) {
  {
    const CameraSensorInfo InfoOrig(mBlackLevel, mWhiteLevel, mMinIso, mMaxIso,
                                    mBlackLevelSeparate);
    CameraSensorInfo Info(InfoOrig); // NOLINT

    checkHelper(Info);
    checkHelper(Info, InfoOrig);
  }

  {
    const unique_ptr<const CameraSensorInfo> InfoOrig(new CameraSensorInfo(
        mBlackLevel, mWhiteLevel, mMinIso, mMaxIso, mBlackLevelSeparate));
    unique_ptr<CameraSensorInfo> Info(new CameraSensorInfo(*InfoOrig));

    checkHelper(*Info);
    checkHelper(*Info, *InfoOrig);
  }

  {
    const CameraSensorInfo InfoOrig(mBlackLevel, mWhiteLevel, mMinIso, mMaxIso,
                                    mBlackLevelSeparate);
    unique_ptr<CameraSensorInfo> Info(new CameraSensorInfo(InfoOrig));

    checkHelper(*Info);
    checkHelper(*Info, InfoOrig);
  }

  {
    const unique_ptr<const CameraSensorInfo> InfoOrig(new CameraSensorInfo(
        mBlackLevel, mWhiteLevel, mMinIso, mMaxIso, mBlackLevelSeparate));
    CameraSensorInfo Info(*InfoOrig);

    checkHelper(Info);
    checkHelper(Info, *InfoOrig);
  }
}

TEST_P(CameraSensorInfoTestDumb, Assignment) {
  ASSERT_NO_THROW({
    const CameraSensorInfo InfoOrig(mBlackLevel, mWhiteLevel, mMinIso, mMaxIso,
                                    mBlackLevelSeparate);
    CameraSensorInfo Info(0, 0, 0, 0, {0});

    Info = InfoOrig;
  });

  ASSERT_NO_THROW({
    const unique_ptr<const CameraSensorInfo> InfoOrig(new CameraSensorInfo(
        mBlackLevel, mWhiteLevel, mMinIso, mMaxIso, mBlackLevelSeparate));
    unique_ptr<CameraSensorInfo> Info(new CameraSensorInfo(0, 0, 0, 0, {0}));

    *Info = *InfoOrig;
  });

  ASSERT_NO_THROW({
    const CameraSensorInfo InfoOrig(mBlackLevel, mWhiteLevel, mMinIso, mMaxIso,
                                    mBlackLevelSeparate);
    unique_ptr<CameraSensorInfo> Info(new CameraSensorInfo(0, 0, 0, 0, {0}));

    *Info = InfoOrig;
  });

  ASSERT_NO_THROW({
    const unique_ptr<const CameraSensorInfo> InfoOrig(new CameraSensorInfo(
        mBlackLevel, mWhiteLevel, mMinIso, mMaxIso, mBlackLevelSeparate));
    CameraSensorInfo Info(0, 0, 0, 0, {0});

    Info = *InfoOrig;
  });
}

TEST_P(CameraSensorInfoTestDumb, AssignmentGetters) {
  ASSERT_NO_THROW({
    const CameraSensorInfo InfoOrig(mBlackLevel, mWhiteLevel, mMinIso, mMaxIso,
                                    mBlackLevelSeparate);
    CameraSensorInfo Info(0, 0, 0, 0, {0});

    Info = InfoOrig;

    checkHelper(Info);
    checkHelper(Info, InfoOrig);
  });

  ASSERT_NO_THROW({
    const unique_ptr<const CameraSensorInfo> InfoOrig(new CameraSensorInfo(
        mBlackLevel, mWhiteLevel, mMinIso, mMaxIso, mBlackLevelSeparate));
    unique_ptr<CameraSensorInfo> Info(new CameraSensorInfo(0, 0, 0, 0, {0}));

    *Info = *InfoOrig;

    checkHelper(*Info);
    checkHelper(*Info, *InfoOrig);
  });

  ASSERT_NO_THROW({
    const CameraSensorInfo InfoOrig(mBlackLevel, mWhiteLevel, mMinIso, mMaxIso,
                                    mBlackLevelSeparate);
    unique_ptr<CameraSensorInfo> Info(new CameraSensorInfo(0, 0, 0, 0, {0}));

    *Info = InfoOrig;

    checkHelper(*Info);
    checkHelper(*Info, InfoOrig);
  });

  ASSERT_NO_THROW({
    const unique_ptr<const CameraSensorInfo> InfoOrig(new CameraSensorInfo(
        mBlackLevel, mWhiteLevel, mMinIso, mMaxIso, mBlackLevelSeparate));
    CameraSensorInfo Info(0, 0, 0, 0, {0});

    Info = *InfoOrig;

    checkHelper(Info);
    checkHelper(Info, *InfoOrig);
  });
}

// --------------------------------------------------------

struct IsoExpectationsT {
  int mMinIso;
  int Iso;
  int mMaxIso;
  bool isIsoWithin;
  bool isDefault;

  friend std::ostream &operator<<(std::ostream &os,
                                  const IsoExpectationsT &obj) {
    return os << "min ISO: " << obj.mMinIso << "; test iso: " << obj.Iso
              << ", max ISO: " << obj.mMaxIso
              << "; is iso within: " << obj.isIsoWithin
              << "; is default: " << obj.isDefault;
  }
};

static const struct IsoExpectationsT CameraSensorIsoInfos[] = {
    IsoExpectationsT{0, 0, 0, true, true},

    IsoExpectationsT{100, 0, 200, false, false},
    IsoExpectationsT{100, 99, 200, false, false},
    IsoExpectationsT{100, 100, 200, true, false},
    IsoExpectationsT{100, 160, 200, true, false},
    IsoExpectationsT{100, 200, 200, true, false},
    IsoExpectationsT{100, 201, 200, false, false},
    IsoExpectationsT{100, std::numeric_limits<int>::max(), 200, false, false},

    // if max iso == 0, every iso which is >= min iso is within.
    IsoExpectationsT{100, 0, 0, false, false},
    IsoExpectationsT{100, 99, 0, false, false},
    IsoExpectationsT{100, 100, 0, true, false},
    IsoExpectationsT{100, std::numeric_limits<int>::max(), 0, true, false},
};

class CameraSensorInfoTest : public ::testing::TestWithParam<IsoExpectationsT> {
protected:
  CameraSensorInfoTest()
      : data(IsoExpectationsT{-1, -1, -1, false, false}),
        mBlackLevel(std::rand()), // NOLINT do not need crypto-level randomness
        mWhiteLevel(std::rand()), // NOLINT do not need crypto-level randomness
        mBlackLevelSeparate({
            std::rand(), // NOLINT do not need crypto-level randomness
            std::rand(), // NOLINT do not need crypto-level randomness
            std::rand(), // NOLINT do not need crypto-level randomness
            std::rand()  // NOLINT do not need crypto-level randomness
        }) {}
  virtual void SetUp() { data = GetParam(); }

  IsoExpectationsT data;

  int mBlackLevel;
  int mWhiteLevel;
  std::vector<int> mBlackLevelSeparate;
};

INSTANTIATE_TEST_CASE_P(Expectations, CameraSensorInfoTest,
                        testing::ValuesIn(CameraSensorIsoInfos));

TEST_P(CameraSensorInfoTest, IsDefault) {
  CameraSensorInfo Info(mBlackLevel, mWhiteLevel, data.mMinIso, data.mMaxIso,
                        mBlackLevelSeparate);

  ASSERT_NO_THROW({
    if (data.isDefault)
      ASSERT_TRUE(Info.isDefault());
    else
      ASSERT_FALSE(Info.isDefault());
  });
}

TEST_P(CameraSensorInfoTest, isIsoWithin) {
  CameraSensorInfo Info(mBlackLevel, mWhiteLevel, data.mMinIso, data.mMaxIso,
                        mBlackLevelSeparate);

  ASSERT_NO_THROW({
    if (data.isIsoWithin)
      ASSERT_TRUE(Info.isIsoWithin(data.Iso));
    else
      ASSERT_FALSE(Info.isIsoWithin(data.Iso));
  });
}
