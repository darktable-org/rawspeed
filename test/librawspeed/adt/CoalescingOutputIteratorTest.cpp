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
#include "common/Common.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
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

template <typename T>
class CoalescingOutputIteratorTest : public testing::Test {};

TYPED_TEST_SUITE_P(CoalescingOutputIteratorTest);

TYPED_TEST_P(CoalescingOutputIteratorTest, Exhaustive) {
  static constexpr int MaxBytes = 256;

  for (int numInputBytes = 1; numInputBytes <= MaxBytes; ++numInputBytes) {
    std::vector<uint8_t> inputStorage(numInputBytes);
    auto input = Array1DRef(inputStorage.data(), numInputBytes);
    std::iota(input.begin(), input.end(), 0);

    std::vector<TypeParam> outputStorage;
    {
      outputStorage.reserve(implicit_cast<size_t>(
          roundUpDivision(numInputBytes, sizeof(TypeParam))));
      auto iter = CoalescingOutputIterator(std::back_inserter(outputStorage));
      for (const auto& e : input)
        *iter = e;
    }

    auto output = Array1DRef<std::byte>(Array1DRef(
        outputStorage.data(), implicit_cast<int>(outputStorage.size())));
    ASSERT_THAT(output, testing::SizeIs(testing::Ge(input.size())));
    output = output.getBlock(input.size(), /*index=*/0).getAsArray1DRef();
    ASSERT_THAT(output, testing::ContainerEq(Array1DRef<std::byte>(input)));
  }
}

REGISTER_TYPED_TEST_SUITE_P(CoalescingOutputIteratorTest, Exhaustive);

using CoalescedTypes = ::testing::Types<uint16_t, uint32_t, uint64_t>;

INSTANTIATE_TYPED_TEST_SUITE_P(CoalescedTo, CoalescingOutputIteratorTest,
                               CoalescedTypes);

} // namespace

} // namespace rawpeed_test

} // namespace rawspeed
