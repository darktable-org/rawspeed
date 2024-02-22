/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2024 Roman Lebedev

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

#include "adt/PartitioningOutputIterator.h"
#include <cstdint>
#include <utility>
#include <vector>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace rawspeed::rawpeed_test {

namespace {

template <typename T, typename U> struct TypeSpec {
  using WideType = T;
  using PartType = U;
};

template <typename T>
class PartitioningOutputIteratorTest : public testing::Test {};

TYPED_TEST_SUITE_P(PartitioningOutputIteratorTest);

template <typename WideType, typename PartType>
std::pair<WideType, std::vector<PartType>> getInput() {
  static_assert(sizeof(WideType) % sizeof(PartType) == 0);
  // NOLINTNEXTLINE(bugprone-sizeof-expression)
  constexpr int NumParts = sizeof(WideType) / sizeof(PartType);
  WideType wide = 0;
  std::vector<PartType> parts;
  for (int i = 0; i != NumParts; ++i) {
    auto part = static_cast<PartType>(-(1 + i));
    parts.emplace_back(part);
    auto bits = static_cast<WideType>(part) << (bitwidth<PartType>() * i);
    wide |= bits;
  }
  return {wide, parts};
}

TYPED_TEST_P(PartitioningOutputIteratorTest, Exhaustive) {
  auto [wide, partsTrue] =
      getInput<typename TypeParam::WideType, typename TypeParam::PartType>();

  std::vector<typename TypeParam::PartType> output;
  output.reserve(partsTrue.size());
  {
    auto it = PartitioningOutputIterator(std::back_inserter(output));
    *it = wide;
  }

  ASSERT_THAT(output, testing::SizeIs(testing::Eq(partsTrue.size())));
  ASSERT_THAT(output, testing::ContainerEq(partsTrue));
}

REGISTER_TYPED_TEST_SUITE_P(PartitioningOutputIteratorTest, Exhaustive);

using PartitionedPairTypes =
    ::testing::Types<TypeSpec<uint8_t, uint8_t>,

                     TypeSpec<uint16_t, uint8_t>, TypeSpec<uint16_t, uint16_t>,

                     TypeSpec<uint32_t, uint8_t>, TypeSpec<uint32_t, uint16_t>,
                     TypeSpec<uint32_t, uint32_t>,

                     TypeSpec<uint64_t, uint8_t>, TypeSpec<uint64_t, uint16_t>,
                     TypeSpec<uint64_t, uint32_t>,
                     TypeSpec<uint64_t, uint64_t>>;

INSTANTIATE_TYPED_TEST_SUITE_P(PartitionedTo, PartitioningOutputIteratorTest,
                               PartitionedPairTypes);
} // namespace

} // namespace rawspeed::rawpeed_test
