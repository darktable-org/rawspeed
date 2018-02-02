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

#include "io/BitPumpLSB.h" // for BitPumpLSB
#include "common/Common.h" // for uchar8
#include "io/BitStream.h"  // for BitStream
#include "io/Buffer.h"     // for Buffer
#include "io/ByteStream.h" // for ByteStream
#include "io/Endianness.h" // for getHostEndianness, Endianness::big, Endia...
#include <array>           // for array
#include <gtest/gtest.h>   // for Message, AssertionResult, ASSERT_PRED_FOR...

using rawspeed::BitPumpLSB;
using rawspeed::Buffer;
using rawspeed::ByteStream;
using rawspeed::DataBuffer;
using rawspeed::Endianness;

namespace rawspeed_test {

TEST(BitPumpLSBTest, IdentityTest) {
  /* [Byte0 Byte1 Byte2 Byte3] */
  /* Byte: [Bit7 .. Bit0] */
  static const std::array<rawspeed::uchar8, 4> data{0b01001011, 0b10000100,
                                                    0b00100000, 0b11110000};

  const Buffer b(data.data(), data.size());

  for (auto e : {Endianness::little, Endianness::big}) {
    const DataBuffer db(b, e);
    const ByteStream bs(db);

    BitPumpLSB p(bs);
    for (int len = 1; len <= 7; len++)
      ASSERT_EQ(p.getBits(len), 1) << "     Where len: " << len;
  }
}

} // namespace rawspeed_test
