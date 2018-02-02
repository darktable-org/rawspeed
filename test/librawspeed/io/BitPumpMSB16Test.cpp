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

#include "io/BitPumpMSB16.h" // for BitPumpMSB16
#include "common/Common.h"   // for uchar8
#include "io/BitStream.h"    // for BitStream
#include "io/Buffer.h"       // for Buffer
#include "io/ByteStream.h"   // for ByteStream
#include "io/Endianness.h"   // for getHostEndianness, Endianness::big, Endia...
#include <array>             // for array
#include <gtest/gtest.h>     // for Message, AssertionResult, ASSERT_PRED_FOR...

using rawspeed::BitPumpMSB16;
using rawspeed::Buffer;
using rawspeed::ByteStream;
using rawspeed::DataBuffer;
using rawspeed::Endianness;

namespace rawspeed_test {

TEST(BitPumpMSB16Test, IdentityTest) {
  /* [Byte1 Byte0 Byte3 Byte2] */
  /* Byte: [Bit0 .. Bit7] */
  static const std::array<rawspeed::uchar8, 4> data{0b01000010, 0b10100100,
                                                    0b00011111, 0b00001000};

  const Buffer b(data.data(), data.size());

  for (auto e : {Endianness::little, Endianness::big}) {
    const DataBuffer db(b, e);
    const ByteStream bs(db);

    BitPumpMSB16 p(bs);
    for (int len = 1; len <= 7; len++)
      ASSERT_EQ(p.getBits(len), 1) << "     Where len: " << len;
  }
}

} // namespace rawspeed_test
