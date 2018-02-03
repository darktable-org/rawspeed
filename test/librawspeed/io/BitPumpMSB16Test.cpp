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
#include "io/BitPumpTest.h"  // for BitPumpTest
#include <array>             // for array
#include <gtest/gtest.h>     // for Message, AssertionResult, ASSERT_PRED_FOR...

using rawspeed::BitPumpMSB16;

namespace rawspeed_test {

template <>
const std::array<rawspeed::uchar8, 4> BitPumpTest<BitPumpMSB16>::ones = {
    /* [Byte1 Byte0 Byte3 Byte2] */
    /* Byte: [Bit0 .. Bit7] */
    0b01000010, 0b10100100, 0b00011111, 0b00001000};

template <>
const std::array<rawspeed::uchar8, 4> BitPumpTest<BitPumpMSB16>::invOnes = {
    0b00100001, 0b11010010, 0b00001111, 0b00000100};

INSTANTIATE_TYPED_TEST_CASE_P(MSB16, BitPumpTest, BitPumpMSB16);

} // namespace rawspeed_test
