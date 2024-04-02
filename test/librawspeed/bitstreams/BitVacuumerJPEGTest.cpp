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

#include "bitstreams/BitVacuumerJPEG.h"
#include "adt/Array1DRef.h"
#include "adt/Casts.h"
#include "adt/PartitioningOutputIterator.h"
#include "bitstreams/BitStreamerJPEG.h"
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
class BitVacuumerJPEGTest : public ::testing::TestWithParam<valueType> {
protected:
  BitVacuumerJPEGTest() = default;
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

    {RecepieType({{std::make_pair(0xFF, 8)}}), ResultType({{0xFF, 0x00, 0x00, 0x00, 0x00}})},

    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 0)}}), ResultType({{0xFF, 0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 1)}}), ResultType({{0xFF, 0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 2)}}), ResultType({{0xFF, 0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 3)}}), ResultType({{0xFF, 0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 4)}}), ResultType({{0xFF, 0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 5)}}), ResultType({{0xFF, 0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 6)}}), ResultType({{0xFF, 0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 7)}}), ResultType({{0xFF, 0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 8)}}), ResultType({{0xFF, 0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 9)}}), ResultType({{0xFF, 0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 10)}}), ResultType({{0xFF, 0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 11)}}), ResultType({{0xFF, 0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 12)}}), ResultType({{0xFF, 0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 13)}}), ResultType({{0xFF, 0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 14)}}), ResultType({{0xFF, 0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 15)}}), ResultType({{0xFF, 0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 16)}}), ResultType({{0xFF, 0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 17)}}), ResultType({{0xFF, 0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 18)}}), ResultType({{0xFF, 0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 19)}}), ResultType({{0xFF, 0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 20)}}), ResultType({{0xFF, 0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 21)}}), ResultType({{0xFF, 0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 22)}}), ResultType({{0xFF, 0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 23)}}), ResultType({{0xFF, 0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 24)}}), ResultType({{0xFF, 0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 25)}}), ResultType({{0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 26)}}), ResultType({{0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 27)}}), ResultType({{0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 28)}}), ResultType({{0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 29)}}), ResultType({{0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 30)}}), ResultType({{0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 31)}}), ResultType({{0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0xFF, 8), std::make_pair(0x00, 32)}}), ResultType({{0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}})},

    {RecepieType({{std::make_pair(0x00, 0), std::make_pair(0xFF, 8)}}), ResultType({{0xFF, 0x00, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 1), std::make_pair(0xFF, 8)}}), ResultType({{0x7F, 0x80, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 2), std::make_pair(0xFF, 8)}}), ResultType({{0x3F, 0xC0, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 3), std::make_pair(0xFF, 8)}}), ResultType({{0x1F, 0xE0, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 4), std::make_pair(0xFF, 8)}}), ResultType({{0x0F, 0xF0, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 5), std::make_pair(0xFF, 8)}}), ResultType({{0x07, 0xF8, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 6), std::make_pair(0xFF, 8)}}), ResultType({{0x03, 0xFC, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 7), std::make_pair(0xFF, 8)}}), ResultType({{0x01, 0xFE, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 8), std::make_pair(0xFF, 8)}}), ResultType({{0x00, 0xFF, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 9), std::make_pair(0xFF, 8)}}), ResultType({{0x00, 0x7F, 0x80, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 10), std::make_pair(0xFF, 8)}}), ResultType({{0x00, 0x3F, 0xC0, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 11), std::make_pair(0xFF, 8)}}), ResultType({{0x00, 0x1F, 0xE0, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 12), std::make_pair(0xFF, 8)}}), ResultType({{0x00, 0x0F, 0xF0, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 13), std::make_pair(0xFF, 8)}}), ResultType({{0x00, 0x07, 0xF8, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 14), std::make_pair(0xFF, 8)}}), ResultType({{0x00, 0x03, 0xFC, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 15), std::make_pair(0xFF, 8)}}), ResultType({{0x00, 0x01, 0xFE, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 16), std::make_pair(0xFF, 8)}}), ResultType({{0x00, 0x00, 0xFF, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 17), std::make_pair(0xFF, 8)}}), ResultType({{0x00, 0x00, 0x7F, 0x80}})},
    {RecepieType({{std::make_pair(0x00, 18), std::make_pair(0xFF, 8)}}), ResultType({{0x00, 0x00, 0x3F, 0xC0}})},
    {RecepieType({{std::make_pair(0x00, 19), std::make_pair(0xFF, 8)}}), ResultType({{0x00, 0x00, 0x1F, 0xE0}})},
    {RecepieType({{std::make_pair(0x00, 20), std::make_pair(0xFF, 8)}}), ResultType({{0x00, 0x00, 0x0F, 0xF0}})},
    {RecepieType({{std::make_pair(0x00, 21), std::make_pair(0xFF, 8)}}), ResultType({{0x00, 0x00, 0x07, 0xF8}})},
    {RecepieType({{std::make_pair(0x00, 22), std::make_pair(0xFF, 8)}}), ResultType({{0x00, 0x00, 0x03, 0xFC}})},
    {RecepieType({{std::make_pair(0x00, 23), std::make_pair(0xFF, 8)}}), ResultType({{0x00, 0x00, 0x01, 0xFE}})},
    {RecepieType({{std::make_pair(0x00, 24), std::make_pair(0xFF, 8)}}), ResultType({{0x00, 0x00, 0x00, 0xFF, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 25), std::make_pair(0xFF, 8)}}), ResultType({{0x00, 0x00, 0x00, 0x7F, 0x80, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 26), std::make_pair(0xFF, 8)}}), ResultType({{0x00, 0x00, 0x00, 0x3F, 0xC0, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 27), std::make_pair(0xFF, 8)}}), ResultType({{0x00, 0x00, 0x00, 0x1F, 0xE0, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 28), std::make_pair(0xFF, 8)}}), ResultType({{0x00, 0x00, 0x00, 0x0F, 0xF0, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 29), std::make_pair(0xFF, 8)}}), ResultType({{0x00, 0x00, 0x00, 0x07, 0xF8, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 30), std::make_pair(0xFF, 8)}}), ResultType({{0x00, 0x00, 0x00, 0x03, 0xFC, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 31), std::make_pair(0xFF, 8)}}), ResultType({{0x00, 0x00, 0x00, 0x01, 0xFE, 0x00, 0x00, 0x00}})},
    {RecepieType({{std::make_pair(0x00, 32), std::make_pair(0xFF, 8)}}), ResultType({{0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00}})},
    // clang-format on
}};

INSTANTIATE_TEST_SUITE_P(Patterns, BitVacuumerJPEGTest,
                         ::testing::ValuesIn(values));

ResultType synthesizeBitstream(const RecepieType& recepie) {
  ResultType bitstream;

  {
    auto bsInserter = PartitioningOutputIterator(std::back_inserter(bitstream));
    using BitVacuumer = BitVacuumerJPEG<decltype(bsInserter)>;
    auto bv = BitVacuumer(bsInserter);

    for (const auto& e : recepie)
      bv.put(e.first, e.second);
  }

  return bitstream;
}

TEST_P(BitVacuumerJPEGTest, Synthesis) {
  const ResultType bitstream = synthesizeBitstream(recepie);
  ASSERT_THAT(bitstream, testing::ContainerEq(expectedOutput));
}

TEST_P(BitVacuumerJPEGTest, Dissolution) {
  if (expectedOutput.empty())
    return;

  if (constexpr int MinSize =
          BitStreamerTraits<BitStreamerJPEG>::MaxProcessBytes;
      expectedOutput.size() < MinSize)
    expectedOutput.resize(MinSize, uint8_t(0));

  auto bs = BitStreamerJPEG(Array1DRef(
      expectedOutput.data(), implicit_cast<int>(expectedOutput.size())));
  for (int i = 0; i != implicit_cast<int>(recepie.size()); ++i) {
    const auto [expectedVal, len] = recepie[i];
    bs.fill(32);
    const auto actualVal = len != 0 ? bs.getBitsNoFill(len) : 0;
    ASSERT_THAT(actualVal, expectedVal);
  }
}

} // namespace

} // namespace rawspeed

// NOTE: no `#pragma GCC diagnostic pop` wanted!
