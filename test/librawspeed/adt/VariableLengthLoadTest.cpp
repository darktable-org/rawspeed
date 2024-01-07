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

#include "adt/VariableLengthLoad.h"
#include "adt/Array1DRef.h"
#include <algorithm>
#include <numeric>
#include <ostream>
#include <vector>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace rawspeed {

template <typename T>
bool operator==(const rawspeed::Array1DRef<T> a,
                const rawspeed::Array1DRef<T> b) {
  return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin());
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

} // namespace rawspeed

using rawspeed::Array1DRef;

namespace rawpeed_test {

TEST(VariableLengthLoadTest, Exhaustive) {
  static constexpr int MaxBytes = 256;

  for (int numInputBytes = 1; numInputBytes <= MaxBytes; ++numInputBytes) {
    std::vector<unsigned char> inputStorage(numInputBytes);
    auto input = Array1DRef(inputStorage.data(), numInputBytes);
    std::iota(input.begin(), input.end(), 0);

    for (int numOutputBytes = 1;
         numOutputBytes <= numInputBytes && numOutputBytes <= 8;
         numOutputBytes *= 2) {
      for (int inPos = 0; inPos <= 4 * numInputBytes; ++inPos) {
        std::vector<unsigned char> outputReferenceStorage(numOutputBytes);
        auto outputReference =
            Array1DRef(outputReferenceStorage.data(), numOutputBytes);
        std::fill(outputReference.begin(), outputReference.end(), 0);
        std::iota(outputReference.begin(),
                  outputReference.addressOf(
                      std::clamp(numInputBytes - inPos, 0, numOutputBytes)),
                  inPos);

        std::vector<unsigned char> outputImpl0Storage(numOutputBytes);
        auto outputImpl0 =
            Array1DRef(outputImpl0Storage.data(), numOutputBytes);
        variableLengthLoadNaiveViaMemcpy(
            outputImpl0, Array1DRef<const unsigned char>(input), inPos);

        EXPECT_THAT(outputImpl0, testing::ContainerEq(outputReference));

        std::vector<unsigned char> outputImpl1Storage(numOutputBytes);
        auto outputImpl1 =
            Array1DRef(outputImpl1Storage.data(), numOutputBytes);
        variableLengthLoadNaiveViaConditionalLoad(
            outputImpl1, Array1DRef<const unsigned char>(input), inPos);

        EXPECT_THAT(outputImpl1, testing::ContainerEq(outputReference));

        std::vector<unsigned char> outputImpl2Storage(numOutputBytes);
        auto outputImpl2 =
            Array1DRef(outputImpl2Storage.data(), numOutputBytes);
        variableLengthLoad(outputImpl2, Array1DRef<const unsigned char>(input),
                           inPos);

        EXPECT_THAT(outputImpl2, testing::ContainerEq(outputReference));
      }
    }
  }
}

} // namespace rawpeed_test
