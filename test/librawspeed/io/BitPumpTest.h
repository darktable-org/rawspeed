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

template <typename T, typename Tag> struct BitPumpPatternTest {};

struct TestGetBitsTag;
template <typename T> struct BitPumpPatternTest<T, TestGetBitsTag> {
  using PumpT = typename T::PumpT;
  using PatternT = typename T::PatternT;

  template <typename L> static void Test(PumpT* pump, L gen) {
    for (int len = 1; len <= 7; len++)
      ASSERT_EQ(pump->getBits(len), gen(len)) << "     Where len: " << len;
  }
};

struct TestGetBitsNoFillTag;
template <typename T> struct BitPumpPatternTest<T, TestGetBitsNoFillTag> {
  using PumpT = typename T::PumpT;
  using PatternT = typename T::PatternT;

  template <typename L> static void Test(PumpT* pump, L gen) {
    pump->fill(32); // Actually fills 32 bits
    for (int len = 1; len <= 7; len++)
      ASSERT_EQ(pump->getBitsNoFill(len), gen(len))
          << "     Where len: " << len;
  }
};

struct TestPeekBitsTag;
template <typename T> struct BitPumpPatternTest<T, TestPeekBitsTag> {
  using PumpT = typename T::PumpT;
  using PatternT = typename T::PatternT;

  template <typename L> static void Test(PumpT* pump, L gen) {
    for (int len = 1; len <= 7; len++) {
      ASSERT_EQ(pump->peekBits(len), gen(len)) << "     Where len: " << len;
      pump->skipBitsNoFill(len);
    }
  }
};

struct TestPeekBitsNoFillTag;
template <typename T> struct BitPumpPatternTest<T, TestPeekBitsNoFillTag> {
  using PumpT = typename T::PumpT;
  using PatternT = typename T::PatternT;

  template <typename L> static void Test(PumpT* pump, L gen) {
    pump->fill(32); // Actually fills 32 bits
    for (int len = 1; len <= 7; len++) {
      ASSERT_EQ(pump->peekBitsNoFill(len), gen(len))
          << "     Where len: " << len;
      pump->skipBitsNoFill(len);
    }
  }
};

struct TestIncreasingPeekLengthTag;
template <typename T>
struct BitPumpPatternTest<T, TestIncreasingPeekLengthTag> {
  using PumpT = typename T::PumpT;
  using PatternT = typename T::PatternT;

  template <typename L> static void Test(PumpT* pump, L data) {
    static const auto MaxLen = 28;
    for (int len = 1; len <= MaxLen; len++)
      ASSERT_EQ(pump->peekBits(len), data(len)) << "     Where len: " << len;
  }
};

struct TestIncreasingPeekLengthNoFillTag;
template <typename T>
struct BitPumpPatternTest<T, TestIncreasingPeekLengthNoFillTag> {
  using PumpT = typename T::PumpT;
  using PatternT = typename T::PatternT;

  template <typename L> static void Test(PumpT* pump, L data) {
    static const auto MaxLen = 28;
    pump->fill(MaxLen); // Actually fills 32 bits
    for (int len = 1; len <= MaxLen; len++)
      ASSERT_EQ(pump->peekBitsNoFill(len), data(len))
          << "     Where len: " << len;
  }
};

template <typename T> class BitPumpTest : public ::testing::Test {
public:
  using PumpT = typename T::PumpT;
  using PatternT = typename T::PatternT;

protected:
  template <typename Tag, typename TestDataType, typename L>
  void runTest(const TestDataType& data, L gen) {
    const Buffer b(data.data(), data.size());

    for (auto e : {Endianness::little, Endianness::big}) {
      const DataBuffer db(b, e);
      const ByteStream bs(db);

      PumpT pump(bs);
      BitPumpPatternTest<T, Tag>::Test(&pump, gen);
    }
  }
};

TYPED_TEST_CASE_P(BitPumpTest);

TYPED_TEST_P(BitPumpTest, GetTest) {
  this->template runTest<TestGetBitsTag>(TypeParam::PatternT::Data,
                                         TypeParam::PatternT::element);
}
TYPED_TEST_P(BitPumpTest, GetNoFillTest) {
  this->template runTest<TestGetBitsNoFillTag>(TypeParam::PatternT::Data,
                                               TypeParam::PatternT::element);
}
TYPED_TEST_P(BitPumpTest, PeekTest) {
  this->template runTest<TestPeekBitsTag>(TypeParam::PatternT::Data,
                                          TypeParam::PatternT::element);
}
TYPED_TEST_P(BitPumpTest, PeekNoFillTest) {
  this->template runTest<TestPeekBitsNoFillTag>(TypeParam::PatternT::Data,
                                                TypeParam::PatternT::element);
}
TYPED_TEST_P(BitPumpTest, IncreasingPeekLengthTest) {
  this->template runTest<TestIncreasingPeekLengthTag>(
      TypeParam::PatternT::Data, TypeParam::PatternT::data);
}
TYPED_TEST_P(BitPumpTest, IncreasingPeekLengthNoFillTest) {
  this->template runTest<TestIncreasingPeekLengthNoFillTag>(
      TypeParam::PatternT::Data, TypeParam::PatternT::data);
}

REGISTER_TYPED_TEST_CASE_P(BitPumpTest, GetTest, GetNoFillTest, PeekTest,
                           PeekNoFillTest, IncreasingPeekLengthTest,
                           IncreasingPeekLengthNoFillTest);

template <typename Pump, typename PatternTag> struct Pattern {};

struct ZerosTag;
template <typename Pump> struct Pattern<Pump, ZerosTag> {
  static const std::array<rawspeed::uchar8, 4> Data;
  static rawspeed::uint32 element(int index) { return 0U; }
  static rawspeed::uint32 data(int len) { return 0U; }
};
template <typename Pump>
const std::array<rawspeed::uchar8, 4> Pattern<Pump, ZerosTag>::Data{
    {/* zero-init */}};

struct OnesTag;
template <typename Pump> struct Pattern<Pump, OnesTag> {
  static const std::array<rawspeed::uchar8, 4> Data;
  static rawspeed::uint32 element(int index) { return 1U; }
  static rawspeed::uint32 data(int len);
};

struct InvOnesTag;
template <typename Pump> struct Pattern<Pump, InvOnesTag> {
  static const std::array<rawspeed::uchar8, 4> Data;
  static rawspeed::uint32 element(int index) { return 1U << (index - 1U); }
  static rawspeed::uint32 data(int len);
};

struct SaturatedTag;
template <typename Pump> struct Pattern<Pump, SaturatedTag> {
  static const std::array<rawspeed::uchar8, 8> Data;
  static rawspeed::uint32 element(int index) { return (1U << index) - 1U; }
  static rawspeed::uint32 data(int len) { return (1U << len) - 1U; }
};
template <typename Pump>
const std::array<rawspeed::uchar8, 8> Pattern<Pump, SaturatedTag>::Data{
    {rawspeed::uchar8(~0U), rawspeed::uchar8(~0U), rawspeed::uchar8(~0U),
     rawspeed::uchar8(~0U)}};

auto GenOnesLE = [](int zerosToOutput,
                    int zerosOutputted) -> std::array<rawspeed::uint32, 29> {
  std::array<rawspeed::uint32, 29> v;
  rawspeed::uint32 bits = 0;
  int currBit = -1;
  for (auto& value : v) {
    if (zerosToOutput == zerosOutputted) {
      assert(currBit < 32);
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
  rawspeed::uint32 bits = 0;
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

template <typename Pump, typename Pattern> struct PumpAndPattern {
  using PumpT = Pump;
  using PatternT = Pattern;
};

template <typename Pump>
using Patterns =
    ::testing::Types<PumpAndPattern<Pump, Pattern<Pump, ZerosTag>>,
                     PumpAndPattern<Pump, Pattern<Pump, OnesTag>>,
                     PumpAndPattern<Pump, Pattern<Pump, InvOnesTag>>,
                     PumpAndPattern<Pump, Pattern<Pump, SaturatedTag>>>;

} // namespace rawspeed_test
