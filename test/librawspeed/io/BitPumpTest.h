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
  template <typename L> void test(const TestDataType& data, L gen) {
    const Buffer b(data.data(), data.size());

    for (auto e : {Endianness::little, Endianness::big}) {
      const DataBuffer db(b, e);
      const ByteStream bs(db);

      T pump(bs);
      for (int len = 1; len <= 7; len++)
        ASSERT_EQ(pump.getBits(len), gen(len)) << "     Where len: " << len;
    }
  }

  static TestDataType ones;
  static TestDataType invOnes;
};

TYPED_TEST_CASE_P(BitPumpTest);

TYPED_TEST_P(BitPumpTest, ReadOnesTest) {
  // I.e. expected values are: "1" "01" "001" ...
  this->test(this->ones, [](int i) -> unsigned { return 1U; });
}

TYPED_TEST_P(BitPumpTest, ReadInvOnesTest) {
  // I.e. expected values are: "1" "10" "100" ...
  this->test(this->invOnes, [](int i) -> unsigned { return 1U << (i - 1); });
}

REGISTER_TYPED_TEST_CASE_P(BitPumpTest, ReadOnesTest, ReadInvOnesTest);

} // namespace rawspeed_test
