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

#include "adt/Array1DRef.h"
#include "io/Buffer.h"
#include "io/ByteStream.h"
#include "io/Endianness.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <gtest/gtest.h>

using rawspeed::Buffer;
using rawspeed::ByteStream;
using rawspeed::DataBuffer;
using rawspeed::Endianness;

namespace rawspeed_test {

template <typename T, typename Tag> struct BitStreamerPatternTest final {};

struct TestGetBitsTag;

template <typename T> struct BitStreamerPatternTest<T, TestGetBitsTag> {
  using PumpT = typename T::PumpT;
  using PatternT = typename T::PatternT;

  template <typename L> static void Test(PumpT& pump, L gen) {
    for (int len = 1; len <= 7; len++)
      ASSERT_EQ(pump.getBits(len), gen(len)) << "     Where len: " << len;
  }
};

struct TestGetBitsNoFillTag;

template <typename T> struct BitStreamerPatternTest<T, TestGetBitsNoFillTag> {
  using PumpT = typename T::PumpT;
  using PatternT = typename T::PatternT;

  template <typename L> static void Test(PumpT& pump, L gen) {
    pump.fill(32); // Actually fills 32 bits
    for (int len = 1; len <= 7; len++)
      ASSERT_EQ(pump.getBitsNoFill(len), gen(len)) << "     Where len: " << len;
  }
};

struct TestPeekBitsTag;

template <typename T> struct BitStreamerPatternTest<T, TestPeekBitsTag> {
  using PumpT = typename T::PumpT;
  using PatternT = typename T::PatternT;

  template <typename L> static void Test(PumpT& pump, L gen) {
    for (int len = 1; len <= 7; len++) {
      ASSERT_EQ(pump.peekBits(len), gen(len)) << "     Where len: " << len;
      pump.skipBitsNoFill(len);
    }
  }
};

struct TestPeekBitsNoFillTag;

template <typename T> struct BitStreamerPatternTest<T, TestPeekBitsNoFillTag> {
  using PumpT = typename T::PumpT;
  using PatternT = typename T::PatternT;

  template <typename L> static void Test(PumpT& pump, L gen) {
    pump.fill(32); // Actually fills 32 bits
    for (int len = 1; len <= 7; len++) {
      ASSERT_EQ(pump.peekBitsNoFill(len), gen(len))
          << "     Where len: " << len;
      pump.skipBitsNoFill(len);
    }
  }
};

struct TestIncreasingPeekLengthTag;

template <typename T>
struct BitStreamerPatternTest<T, TestIncreasingPeekLengthTag> {
  using PumpT = typename T::PumpT;
  using PatternT = typename T::PatternT;

  template <typename L> static void Test(PumpT& pump, L data) {
    static const auto MaxLen = 28;
    for (int len = 1; len <= MaxLen; len++)
      ASSERT_EQ(pump.peekBits(len), data(len)) << "     Where len: " << len;
  }
};

struct TestIncreasingPeekLengthNoFillTag;

template <typename T>
struct BitStreamerPatternTest<T, TestIncreasingPeekLengthNoFillTag> {
  using PumpT = typename T::PumpT;
  using PatternT = typename T::PatternT;

  template <typename L> static void Test(PumpT& pump, L data) {
    static const auto MaxLen = 28;
    pump.fill(MaxLen); // Actually fills 32 bits
    for (int len = 1; len <= MaxLen; len++)
      ASSERT_EQ(pump.peekBitsNoFill(len), data(len))
          << "     Where len: " << len;
  }
};

template <typename T> class BitStreamerTest : public ::testing::Test {
public:
  using PumpT = typename T::PumpT;
  using PatternT = typename T::PatternT;

protected:
  template <typename Tag, typename TestDataType, typename L>
  void runTest(const TestDataType& data, L gen) {
    const rawspeed::Array1DRef<const uint8_t> input(
        data.data(), rawspeed::implicit_cast<int>(data.size()));

    PumpT pump(input);
    BitStreamerPatternTest<T, Tag>::Test(pump, gen);
  }
};

TYPED_TEST_SUITE_P(BitStreamerTest);

TYPED_TEST_P(BitStreamerTest, GetTest) {
  this->template runTest<TestGetBitsTag>(TypeParam::PatternT::Data,
                                         TypeParam::PatternT::element);
}
TYPED_TEST_P(BitStreamerTest, GetNoFillTest) {
  this->template runTest<TestGetBitsNoFillTag>(TypeParam::PatternT::Data,
                                               TypeParam::PatternT::element);
}
TYPED_TEST_P(BitStreamerTest, PeekTest) {
  this->template runTest<TestPeekBitsTag>(TypeParam::PatternT::Data,
                                          TypeParam::PatternT::element);
}
TYPED_TEST_P(BitStreamerTest, PeekNoFillTest) {
  this->template runTest<TestPeekBitsNoFillTag>(TypeParam::PatternT::Data,
                                                TypeParam::PatternT::element);
}
TYPED_TEST_P(BitStreamerTest, IncreasingPeekLengthTest) {
  this->template runTest<TestIncreasingPeekLengthTag>(
      TypeParam::PatternT::Data, TypeParam::PatternT::data);
}
TYPED_TEST_P(BitStreamerTest, IncreasingPeekLengthNoFillTest) {
  this->template runTest<TestIncreasingPeekLengthNoFillTag>(
      TypeParam::PatternT::Data, TypeParam::PatternT::data);
}

REGISTER_TYPED_TEST_SUITE_P(BitStreamerTest, GetTest, GetNoFillTest, PeekTest,
                            PeekNoFillTest, IncreasingPeekLengthTest,
                            IncreasingPeekLengthNoFillTest);

template <typename Pump, typename PatternTag> struct Pattern final {};

struct ZerosTag;

template <typename Pump> struct Pattern<Pump, ZerosTag> {
  static const std::array<uint8_t, 8> Data;
  static uint32_t element(int index) { return 0U; }
  static uint32_t data(int len) { return 0U; }
};
template <typename Pump>
const std::array<uint8_t, 8> Pattern<Pump, ZerosTag>::Data{{/* zero-init */}};

struct OnesTag;

template <typename Pump> struct Pattern<Pump, OnesTag> {
  static const std::array<uint8_t, 8> Data;
  static uint32_t element(int index) { return 1U; }
  static uint32_t data(int len);
};

struct InvOnesTag;

template <typename Pump> struct Pattern<Pump, InvOnesTag> {
  static const std::array<uint8_t, 8> Data;
  static uint32_t element(int index) { return 1U << (index - 1U); }
  static uint32_t data(int len);
};

struct SaturatedTag;

template <typename Pump> struct Pattern<Pump, SaturatedTag> {
  static const std::array<uint8_t, 8> Data;
  static uint32_t element(int index) { return (1U << index) - 1U; }
  static uint32_t data(int len) { return (1U << len) - 1U; }
};
template <typename Pump>
const std::array<uint8_t, 8> Pattern<Pump, SaturatedTag>::Data{
    {uint8_t(~0U), uint8_t(~0U), uint8_t(~0U), uint8_t(~0U)}};

auto GenOnesLE = [](int zerosToOutput,
                    int zerosOutputted) -> std::array<uint32_t, 29> {
  std::array<uint32_t, 29> v;
  uint32_t bits = 0;
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
                    int zerosOutputted) -> std::array<uint32_t, 29> {
  std::array<uint32_t, 29> v;
  uint32_t bits = 0;
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

template <typename Pump, typename Pattern> struct PumpAndPattern final {
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
