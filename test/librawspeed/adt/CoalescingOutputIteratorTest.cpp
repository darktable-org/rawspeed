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

#include "adt/CoalescingOutputIterator.h"
#include "adt/Array1DRef.h"
#include "adt/Casts.h"
#include "common/Common.h"
#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <numeric>
#include <ostream>
#include <vector>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace rawspeed {

template <typename T>
bool operator==(const Array1DRef<T> a, const Array1DRef<T> b) {
  return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin());
}

template <typename T>
  requires std::same_as<T, std::byte>
inline ::std::ostream& operator<<(::std::ostream& os, const T& b) {
  os << std::to_integer<uint8_t>(b);
  return os;
}

template <typename T>
::std::ostream& operator<<(::std::ostream& os, const Array1DRef<T>& r) {
  os << "{";
  for (int i = 0; i != r.size(); ++i) {
    if (i != 0)
      os << ", ";
    os << r(i);
  }
  os << "}";
  return os;
}

namespace rawpeed_test {

namespace {

template <typename CoalescedType, typename PartType>
auto coalesceElts(Array1DRef<const PartType> input) {
  std::vector<CoalescedType> outputStorage;
  {
    outputStorage.reserve(implicit_cast<size_t>(roundUpDivisionSafe(
        sizeof(PartType) * input.size(), sizeof(CoalescedType))));
    auto subIter = std::back_inserter(outputStorage);
    auto iter = CoalescingOutputIterator<decltype(subIter), PartType>(subIter);
    static_assert(std::output_iterator<decltype(iter), PartType>);
    for (const auto& e : input)
      *iter = e;
  }
  return outputStorage;
}

template <typename T, typename U> struct TypeSpec {
  using CoalescedType = T;
  using PartType = U;
};

template <typename T>
class CoalescingOutputIteratorTest : public testing::Test {};

TYPED_TEST_SUITE_P(CoalescingOutputIteratorTest);

TYPED_TEST_P(CoalescingOutputIteratorTest, Exhaustive) {
  static constexpr int MaxBytes = 256;

  for (int numInputBytes = 1; numInputBytes <= MaxBytes; ++numInputBytes) {
    std::vector<uint8_t> inputStorage(numInputBytes);
    auto input = Array1DRef(inputStorage.data(), numInputBytes);
    std::iota(input.begin(), input.end(), 0);

    std::vector<typename TypeParam::PartType> intermediateStorage =
        coalesceElts<typename TypeParam::PartType, uint8_t>(input);
    auto intermediate =
        Array1DRef(intermediateStorage.data(),
                   implicit_cast<int>(intermediateStorage.size()));

    auto intermediateBytes = Array1DRef<std::byte>(intermediate);

    ASSERT_THAT(intermediateBytes, testing::SizeIs(testing::Ge(input.size())));
    intermediateBytes =
        intermediateBytes.getBlock(input.size(), /*index=*/0).getAsArray1DRef();
    ASSERT_THAT(intermediateBytes,
                testing::ContainerEq(Array1DRef<std::byte>(input)));

    std::vector<typename TypeParam::CoalescedType> outputStorage =
        coalesceElts<typename TypeParam::CoalescedType,
                     typename TypeParam::PartType>(intermediate);

    auto output = Array1DRef<std::byte>(Array1DRef(
        outputStorage.data(), implicit_cast<int>(outputStorage.size())));
    ASSERT_THAT(output, testing::SizeIs(testing::Ge(input.size())));
    output = output.getBlock(input.size(), /*index=*/0).getAsArray1DRef();
    ASSERT_THAT(output, testing::ContainerEq(Array1DRef<std::byte>(input)));
  }
}

REGISTER_TYPED_TEST_SUITE_P(CoalescingOutputIteratorTest, Exhaustive);

using CoalescedPairTypes =
    ::testing::Types<TypeSpec<uint8_t, uint8_t>,

                     TypeSpec<uint16_t, uint8_t>, TypeSpec<uint16_t, uint16_t>,

                     TypeSpec<uint32_t, uint8_t>, TypeSpec<uint32_t, uint16_t>,
                     TypeSpec<uint32_t, uint32_t>,

                     TypeSpec<uint64_t, uint8_t>, TypeSpec<uint64_t, uint16_t>,
                     TypeSpec<uint64_t, uint32_t>,
                     TypeSpec<uint64_t, uint64_t>>;

INSTANTIATE_TYPED_TEST_SUITE_P(CoalescedTo, CoalescingOutputIteratorTest,
                               CoalescedPairTypes);
} // namespace

} // namespace rawpeed_test

} // namespace rawspeed
