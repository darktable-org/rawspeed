/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2018 Roman Lebedev

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

#include "common/Common.h" // for uchar8
#include "io/Buffer.h"     // for Buffer
#include "io/ByteStream.h" // for ByteStream
#include "io/Endianness.h" // for getHostEndianness, Endianness::big, Endia...
#include <array>           // for array
#include <gtest/gtest.h>   // for Message, AssertionResult, ASSERT_PRED_FOR...

using rawspeed::Buffer;
using rawspeed::ByteStream;
using rawspeed::DataBuffer;
using rawspeed::Endianness;

namespace rawspeed_test {

template <typename T> class BitPumpTest : public ::testing::Test {
public:
  using TestDataType = const std::array<rawspeed::uchar8, 4>;

protected:
  static constexpr auto TestGetBits = [](T* pump, auto gen) -> void {
    for (int len = 1; len <= 7; len++)
      ASSERT_EQ(pump->getBits(len), gen(len)) << "     Where len: " << len;
  };

  static constexpr auto TestPeekBits = [](T* pump, auto gen) -> void {
    for (int len = 1; len <= 7; len++) {
      ASSERT_EQ(pump->peekBits(len), gen(len)) << "     Where len: " << len;
      pump->skipBits(len);
    }
  };

  static constexpr auto TestIncreasingPeekLength = [](T* pump,
                                                      auto data) -> void {
    static constexpr auto MaxLen = 28;
    for (int len = 1; len <= MaxLen; len++)
      ASSERT_EQ(pump->peekBits(len), data[len]) << "     Where len: " << len;
  };

  template <typename Test, typename L>
  void runTest(const TestDataType& data, Test test, L gen) {
    const Buffer b(data.data(), data.size());

    for (auto e : {Endianness::little, Endianness::big}) {
      const DataBuffer db(b, e);
      const ByteStream bs(db);

      T pump(bs);
      test(&pump, gen);
    }
  }

  static TestDataType ones;
  // I.e. expected values are: "1" "01" "001" ...
  static constexpr auto onesExpected = [](int i) -> unsigned { return 1U; };

  static TestDataType invOnes;
  // I.e. expected values are: "1" "10" "100" ...
  static constexpr auto invOnesExpected = [](int i) -> unsigned {
    return 1U << (i - 1);
  };

  static const std::array<rawspeed::uint32, 29> IncreasingPeekLengthOnesDataLE;
  static const std::array<rawspeed::uint32, 29> IncreasingPeekLengthOnesDataBE;

  static const std::array<rawspeed::uint32, 29>
      IncreasingPeekLengthInvOnesDataLE;
  static const std::array<rawspeed::uint32, 29>
      IncreasingPeekLengthInvOnesDataBE;

  static const std::array<rawspeed::uint32, 29>& IncreasingPeekLengthOnesData;
  static const std::array<rawspeed::uint32, 29>&
      IncreasingPeekLengthInvOnesData;
};

auto GenOnesLE = [](int zerosToOutput,
                    int zerosOutputted) -> std::array<rawspeed::uint32, 29> {
  std::array<rawspeed::uint32, 29> v;
  unsigned bits = 0;
  int currBit = -1;
  for (auto& value : v) {
    if (zerosToOutput == zerosOutputted) {
      bits |= 0b1 << currBit;
      zerosToOutput++;
      zerosOutputted = 0;
    }
    value = bits;
    zerosOutputted++;
    currBit++;
  }
  return v;
};

auto GenOnesBE = [](int zerosToOutput,
                    int zerosOutputted) -> std::array<rawspeed::uint32, 29> {
  std::array<rawspeed::uint32, 29> v;
  unsigned bits = 0;
  for (auto& value : v) {
    if (zerosToOutput == zerosOutputted) {
      bits |= 0b1;
      zerosToOutput++;
      zerosOutputted = 0;
    }
    value = bits;
    zerosOutputted++;
    bits <<= 1;
  }
  return v;
};

template <typename T>
const std::array<rawspeed::uint32, 29>
    BitPumpTest<T>::IncreasingPeekLengthOnesDataLE = GenOnesLE(0, -1);

template <typename T>
const std::array<rawspeed::uint32, 29>
    BitPumpTest<T>::IncreasingPeekLengthOnesDataBE = GenOnesBE(1, 0);

template <typename T>
const std::array<rawspeed::uint32, 29>
    BitPumpTest<T>::IncreasingPeekLengthInvOnesDataLE = GenOnesLE(1, 0);

template <typename T>
const std::array<rawspeed::uint32, 29>
    BitPumpTest<T>::IncreasingPeekLengthInvOnesDataBE = GenOnesBE(0, -1);

TYPED_TEST_CASE_P(BitPumpTest);

TYPED_TEST_P(BitPumpTest, GetOnesTest) {
  this->runTest(this->ones, this->TestGetBits, this->onesExpected);
}
TYPED_TEST_P(BitPumpTest, PeekOnesTest) {
  this->runTest(this->ones, this->TestPeekBits, this->onesExpected);
}
TYPED_TEST_P(BitPumpTest, IncreasingPeekLengthOnesTest) {
  this->runTest(this->ones, this->TestIncreasingPeekLength,
                this->IncreasingPeekLengthOnesData);
}

TYPED_TEST_P(BitPumpTest, GetInvOnesTest) {
  this->runTest(this->invOnes, this->TestGetBits, this->invOnesExpected);
}
TYPED_TEST_P(BitPumpTest, PeekInvOnesTest) {
  this->runTest(this->invOnes, this->TestPeekBits, this->invOnesExpected);
}
TYPED_TEST_P(BitPumpTest, IncreasingPeekLengthInvOnesTest) {
  this->runTest(this->invOnes, this->TestIncreasingPeekLength,
                this->IncreasingPeekLengthInvOnesData);
}

REGISTER_TYPED_TEST_CASE_P(BitPumpTest,

                           GetOnesTest, PeekOnesTest,
                           IncreasingPeekLengthOnesTest,

                           GetInvOnesTest, PeekInvOnesTest,
                           IncreasingPeekLengthInvOnesTest);

} // namespace rawspeed_test
