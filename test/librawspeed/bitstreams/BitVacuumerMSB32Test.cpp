/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2024 Roman Lebedev

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

#include "bitstreams/BitVacuumerMSB32.h"
#include "adt/Array1DRef.h"
#include "adt/Bit.h"
#include "adt/Casts.h"
#include "adt/PartitioningOutputIterator.h"
#include "bitstreams/BitStreamPosition.h"
#include "bitstreams/BitStreamerMSB32.h"
#include "common/Common.h"
#include <climits>
#include <cstdint>
#include <iterator>
#include <utility>
#include <vector>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wframe-larger-than="
#pragma GCC diagnostic ignored "-Wstack-usage="

namespace rawspeed {

namespace {

using RecepieEntryType = std::pair<uint32_t, uint8_t>;
using RecepieType = std::vector<RecepieEntryType>;
using ResultType = std::vector<uint8_t>;

using valueType = std::pair<RecepieType, ResultType>;
class BitVacuumerMSB32Test : public ::testing::TestWithParam<valueType> {
protected:
  BitVacuumerMSB32Test() = default;
  virtual void SetUp() {
    recepie = std::get<0>(GetParam());
    expectedOutput = std::get<1>(GetParam());
  }

  RecepieType recepie;
  ResultType expectedOutput;
};
const std::vector<valueType> values = {{
    // clang-format off
    {RecepieType({{std::make_pair(0x00, 0)}}), ResultType()},
    {RecepieType({{std::make_pair(0x00, 1)}}), ResultType({{0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 2)}}), ResultType({{0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 3)}}), ResultType({{0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 4)}}), ResultType({{0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 5)}}), ResultType({{0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 6)}}), ResultType({{0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 7)}}), ResultType({{0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 8)}}), ResultType({{0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 9)}}), ResultType({{0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 10)}}), ResultType({{0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 11)}}), ResultType({{0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 12)}}), ResultType({{0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 13)}}), ResultType({{0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 14)}}), ResultType({{0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 15)}}), ResultType({{0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 16)}}), ResultType({{0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 17)}}), ResultType({{0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 18)}}), ResultType({{0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 19)}}), ResultType({{0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 20)}}), ResultType({{0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 21)}}), ResultType({{0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 22)}}), ResultType({{0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 23)}}), ResultType({{0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 24)}}), ResultType({{0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 25)}}), ResultType({{0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 26)}}), ResultType({{0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 27)}}), ResultType({{0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 28)}}), ResultType({{0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 29)}}), ResultType({{0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 30)}}), ResultType({{0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 31)}}), ResultType({{0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 32)}}), ResultType({{0x00, 0x00, 0x00, 0x00}})},

    {RecepieType({{std::make_pair(0xFF, 8)}}), ResultType({{0x00, 0x00, 0x00, 0xFF}})},

    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 0)}}), ResultType({{0x00, 0x00, 0x00, 0xFF}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 1)}}), ResultType({{0x00, 0x00, 0x00, 0xFF}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 2)}}), ResultType({{0x00, 0x00, 0x00, 0xFF}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 3)}}), ResultType({{0x00, 0x00, 0x00, 0xFF}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 4)}}), ResultType({{0x00, 0x00, 0x00, 0xFF}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 5)}}), ResultType({{0x00, 0x00, 0x00, 0xFF}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 6)}}), ResultType({{0x00, 0x00, 0x00, 0xFF}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 7)}}), ResultType({{0x00, 0x00, 0x00, 0xFF}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 8)}}), ResultType({{0x00, 0x00, 0x00, 0xFF}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 9)}}), ResultType({{0x00, 0x00, 0x00, 0xFF}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 10)}}), ResultType({{0x00, 0x00, 0x00, 0xFF}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 11)}}), ResultType({{0x00, 0x00, 0x00, 0xFF}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 12)}}), ResultType({{0x00, 0x00, 0x00, 0xFF}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 13)}}), ResultType({{0x00, 0x00, 0x00, 0xFF}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 14)}}), ResultType({{0x00, 0x00, 0x00, 0xFF}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 15)}}), ResultType({{0x00, 0x00, 0x00, 0xFF}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 16)}}), ResultType({{0x00, 0x00, 0x00, 0xFF}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 17)}}), ResultType({{0x00, 0x00, 0x00, 0xFF}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 18)}}), ResultType({{0x00, 0x00, 0x00, 0xFF}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 19)}}), ResultType({{0x00, 0x00, 0x00, 0xFF}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 20)}}), ResultType({{0x00, 0x00, 0x00, 0xFF}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 21)}}), ResultType({{0x00, 0x00, 0x00, 0xFF}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 22)}}), ResultType({{0x00, 0x00, 0x00, 0xFF}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 23)}}), ResultType({{0x00, 0x00, 0x00, 0xFF}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 24)}}), ResultType({{0x00, 0x00, 0x00, 0xFF}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 25)}}), ResultType({{0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 26)}}), ResultType({{0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 27)}}), ResultType({{0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 28)}}), ResultType({{0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 29)}}), ResultType({{0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 30)}}), ResultType({{0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 31)}}), ResultType({{0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 32)}}), ResultType({{0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00}})},

    {RecepieType({{std::make_pair(0x00, 0), std::make_pair(0xFF, 8)}}), ResultType({{0x00, 0x00, 0x00, 0xFF}})},
    {RecepieType({{std::make_pair(0x00, 1), std::make_pair(0xFF, 8)}}), ResultType({{0x00, 0x00, 0x80, 0x7F}})},
    {RecepieType({{std::make_pair(0x00, 2), std::make_pair(0xFF, 8)}}), ResultType({{0x00, 0x00, 0xC0, 0x3F}})},
    {RecepieType({{std::make_pair(0x00, 3), std::make_pair(0xFF, 8)}}), ResultType({{0x00, 0x00, 0xE0, 0x1F}})},
    {RecepieType({{std::make_pair(0x00, 4), std::make_pair(0xFF, 8)}}), ResultType({{0x00, 0x00, 0xF0, 0x0F}})},
    {RecepieType({{std::make_pair(0x00, 5), std::make_pair(0xFF, 8)}}), ResultType({{0x00, 0x00, 0xF8, 0x07}})},
    {RecepieType({{std::make_pair(0x00, 6), std::make_pair(0xFF, 8)}}), ResultType({{0x00, 0x00, 0xFC, 0x03}})},
    {RecepieType({{std::make_pair(0x00, 7), std::make_pair(0xFF, 8)}}), ResultType({{0x00, 0x00, 0xFE, 0x01}})},
    {RecepieType({{std::make_pair(0x00, 8), std::make_pair(0xFF, 8)}}), ResultType({{0x00, 0x00, 0xFF, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 9), std::make_pair(0xFF, 8)}}), ResultType({{0x00, 0x80, 0x7F, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 10), std::make_pair(0xFF, 8)}}), ResultType({{0x00, 0xC0, 0x3F, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 11), std::make_pair(0xFF, 8)}}), ResultType({{0x00, 0xE0, 0x1F, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 12), std::make_pair(0xFF, 8)}}), ResultType({{0x00, 0xF0, 0x0F, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 13), std::make_pair(0xFF, 8)}}), ResultType({{0x00, 0xF8, 0x07, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 14), std::make_pair(0xFF, 8)}}), ResultType({{0x00, 0xFC, 0x03, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 15), std::make_pair(0xFF, 8)}}), ResultType({{0x00, 0xFE, 0x01, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 16), std::make_pair(0xFF, 8)}}), ResultType({{0x00, 0xFF, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 17), std::make_pair(0xFF, 8)}}), ResultType({{0x80, 0x7F, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 18), std::make_pair(0xFF, 8)}}), ResultType({{0xC0, 0x3F, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 19), std::make_pair(0xFF, 8)}}), ResultType({{0xE0, 0x1F, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 20), std::make_pair(0xFF, 8)}}), ResultType({{0xF0, 0x0F, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 21), std::make_pair(0xFF, 8)}}), ResultType({{0xF8, 0x07, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 22), std::make_pair(0xFF, 8)}}), ResultType({{0xFC, 0x03, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 23), std::make_pair(0xFF, 8)}}), ResultType({{0xFE, 0x01, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 24), std::make_pair(0xFF, 8)}}), ResultType({{0xFF, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 25), std::make_pair(0xFF, 8)}}), ResultType({{0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80}})},
    {RecepieType({{std::make_pair(0x00, 26), std::make_pair(0xFF, 8)}}), ResultType({{0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0}})},
    {RecepieType({{std::make_pair(0x00, 27), std::make_pair(0xFF, 8)}}), ResultType({{0x1F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE0}})},
    {RecepieType({{std::make_pair(0x00, 28), std::make_pair(0xFF, 8)}}), ResultType({{0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0}})},
    {RecepieType({{std::make_pair(0x00, 29), std::make_pair(0xFF, 8)}}), ResultType({{0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8}})},
    {RecepieType({{std::make_pair(0x00, 30), std::make_pair(0xFF, 8)}}), ResultType({{0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFC}})},
    {RecepieType({{std::make_pair(0x00, 31), std::make_pair(0xFF, 8)}}), ResultType({{0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFE}})},
    {RecepieType({{std::make_pair(0x00, 32), std::make_pair(0xFF, 8)}}), ResultType({{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF}})},
    // clang-format on
}};

INSTANTIATE_TEST_SUITE_P(Patterns, BitVacuumerMSB32Test,
                         ::testing::ValuesIn(values));

ResultType synthesizeBitstream(const RecepieType& recepie) {
  ResultType bitstream;

  {
    auto bsInserter = PartitioningOutputIterator(std::back_inserter(bitstream));
    using BitVacuumer = BitVacuumerMSB32<decltype(bsInserter)>;
    auto bv = BitVacuumer(bsInserter);

    for (const auto& e : recepie)
      bv.put(e.first, e.second);
  }

  return bitstream;
}

TEST_P(BitVacuumerMSB32Test, Synthesis) {
  const ResultType bitstream = synthesizeBitstream(recepie);
  ASSERT_THAT(bitstream, testing::ContainerEq(expectedOutput));
}

TEST_P(BitVacuumerMSB32Test, Dissolution) {
  if (expectedOutput.empty())
    return;

  auto bs = BitStreamerMSB32(Array1DRef(
      expectedOutput.data(), implicit_cast<int>(expectedOutput.size())));
  for (int i = 0; i != implicit_cast<int>(recepie.size()); ++i) {
    const auto [expectedVal, len] = recepie[i];
    bs.fill(32);
    const auto actualVal = len != 0 ? bs.getBitsNoFill(len) : 0;
    ASSERT_THAT(actualVal, expectedVal);
  }
}

TEST(BitVacuumerMSB32Test, LoadPos) {
  std::vector<uint8_t> bitstream;

  constexpr int numByteElts = 64;

  {
    auto bsInserter = PartitioningOutputIterator(std::back_inserter(bitstream));
    using BitVacuumer = BitVacuumerMSB32<decltype(bsInserter)>;
    auto bv = BitVacuumer(bsInserter);

    for (int e = 0; e != numByteElts; ++e)
      bv.put(e, 8);
  }

  auto fullInput =
      Array1DRef(bitstream.data(), implicit_cast<int>(bitstream.size()));

  using BitStreamer = BitStreamerMSB32;
  using BitStreamerTraits = BitStreamer::Traits;
  using BitStreamTraits = BitStreamer::StreamTraits;

  for (int fillLevel : {8, 32}) {
    for (int baseLoadPosStep = 1;
         baseLoadPosStep <= 2 * BitStreamTraits::MinLoadStepByteMultiple;
         ++baseLoadPosStep) {
      for (int baseLoadPos = 0;
           baseLoadPos <= numByteElts - BitStreamerTraits::MaxProcessBytes;
           baseLoadPos += baseLoadPosStep) {
        auto input =
            fullInput.getCrop(baseLoadPos, fullInput.size() - baseLoadPos)
                .getAsArray1DRef();
        auto bs = BitStreamer(input);
        for (int i = 0; i != input.size(); ++i) {
          const auto expectedVal = baseLoadPos + i;
          bs.fill(fillLevel);
          const auto actualVal = bs.getBitsNoFill(8);
          if (baseLoadPosStep % BitStreamTraits::MinLoadStepByteMultiple == 0 ||
              baseLoadPos % BitStreamTraits::MinLoadStepByteMultiple == 0) {
            ASSERT_THAT(actualVal, expectedVal);
          } else {
            ASSERT_NE(actualVal, expectedVal);
          }
        }
      }
    }
  }
}

TEST(BitVacuumerMSB32Test, DependencyBreaking) {
  std::vector<uint8_t> bitstream;

  using BitStreamer = BitStreamerMSB32;
  using BitStreamerTraits = BitStreamer::Traits;

  constexpr int numByteElts = 256;

  {
    auto bsInserter = PartitioningOutputIterator(std::back_inserter(bitstream));
    using BitVacuumer = BitVacuumerMSB32<decltype(bsInserter)>;
    auto bv = BitVacuumer(bsInserter);

    for (int e = 0; e != numByteElts + BitStreamerTraits::MaxProcessBytes; ++e)
      bv.put(e, 8);
  }
  constexpr int numBitsTotal = CHAR_BIT * numByteElts;

  auto fullInput =
      Array1DRef(bitstream.data(), implicit_cast<int>(bitstream.size()));

  for (int numBitsToSkip = 0; numBitsToSkip <= numBitsTotal; ++numBitsToSkip) {
    auto bsRef = BitStreamer(fullInput);
    bsRef.fill();

    for (int i = 0; i != numBitsToSkip / 8; ++i) {
      const auto expectedVal = i;
      ASSERT_THAT(bsRef.getBits(8), expectedVal);
    }
    if ((numBitsToSkip % 8) != 0) {
      const auto expectedVal = extractHighBits<unsigned>(
          numBitsToSkip / CHAR_BIT, numBitsToSkip % 8, CHAR_BIT);
      ASSERT_THAT(bsRef.getBits(numBitsToSkip % 8), expectedVal);
    }

    const int numBitsRemaining = numBitsTotal - numBitsToSkip;

    BitStreamPosition<BitStreamerTraits::Tag> state;
    state.pos = bsRef.getInputPosition();
    state.fillLevel = bsRef.getFillLevel();
    const auto bsPos = getAsByteStreamPosition(state);

    auto rebasedInput =
        fullInput.getCrop(bsPos.bytePos, fullInput.size() - bsPos.bytePos)
            .getAsArray1DRef();
    auto bsRebased = BitStreamer(rebasedInput);
    if (bsPos.numBitsToSkip != 0)
      bsRebased.skipBits(bsPos.numBitsToSkip);

    int numSubByteBitsRemaining = numBitsRemaining % CHAR_BIT;
    int numBytesRemaining = numBitsRemaining / CHAR_BIT;
    if (numSubByteBitsRemaining != 0) {
      const auto expectedVal = extractLowBits<unsigned>(
          numBitsToSkip / CHAR_BIT, numSubByteBitsRemaining);
      ASSERT_THAT(bsRef.getBits(numSubByteBitsRemaining), expectedVal);
      ASSERT_THAT(bsRebased.getBits(numSubByteBitsRemaining), expectedVal);
    }

    for (int i = 0; i != numBytesRemaining; ++i) {
      const auto expectedVal = roundUpDivision(numBitsToSkip, CHAR_BIT) + i;
      ASSERT_THAT(bsRef.getBits(8), expectedVal);
      ASSERT_THAT(bsRebased.getBits(8), expectedVal);
    }
  }
}

TEST(BitVacuumerMSB32Test, ReloadCache) {
  std::vector<uint8_t> bitstream;

  using BitStreamer = BitStreamerMSB32;
  using BitStreamerTraits = BitStreamer::Traits;

  constexpr int numByteElts = 256;

  {
    auto bsInserter = PartitioningOutputIterator(std::back_inserter(bitstream));
    using BitVacuumer = BitVacuumerMSB32<decltype(bsInserter)>;
    auto bv = BitVacuumer(bsInserter);

    for (int e = 0; e != numByteElts + BitStreamerTraits::MaxProcessBytes; ++e)
      bv.put(e, 8);
  }
  constexpr int numBitsTotal = CHAR_BIT * numByteElts;

  auto fullInput =
      Array1DRef(bitstream.data(), implicit_cast<int>(bitstream.size()));

  for (int numBitsToSkip = 0; numBitsToSkip <= numBitsTotal; ++numBitsToSkip) {
    auto bsRef = BitStreamer(fullInput);
    bsRef.fill();

    for (int i = 0; i != numBitsToSkip / 8; ++i) {
      const auto expectedVal = i;
      ASSERT_THAT(bsRef.getBits(8), expectedVal);
    }
    if ((numBitsToSkip % 8) != 0) {
      const auto expectedVal = extractHighBits<unsigned>(
          numBitsToSkip / CHAR_BIT, numBitsToSkip % 8, CHAR_BIT);
      ASSERT_THAT(bsRef.getBits(numBitsToSkip % 8), expectedVal);
    }

    bsRef.reload();

    const int numBitsRemaining = numBitsTotal - numBitsToSkip;

    int numSubByteBitsRemaining = numBitsRemaining % CHAR_BIT;
    int numBytesRemaining = numBitsRemaining / CHAR_BIT;
    if (numSubByteBitsRemaining != 0) {
      const auto expectedVal = extractLowBits<unsigned>(
          numBitsToSkip / CHAR_BIT, numSubByteBitsRemaining);
      ASSERT_THAT(bsRef.getBits(numSubByteBitsRemaining), expectedVal);
    }

    for (int i = 0; i != numBytesRemaining; ++i) {
      const auto expectedVal = roundUpDivision(numBitsToSkip, CHAR_BIT) + i;
      ASSERT_THAT(bsRef.getBits(8), expectedVal);
    }
  }
}

} // namespace

} // namespace rawspeed

// NOTE: no `#pragma GCC diagnostic pop` wanted!
