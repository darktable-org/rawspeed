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

#include "io/BitPumpLSB.h"  // for BitPumpLSB
#include "common/Common.h"  // for uchar8
#include "io/BitPumpTest.h" // for BitPumpTest
#include <array>            // for array
#include <gtest/gtest.h>    // for Message, AssertionResult, ASSERT_PRED_FOR...

using rawspeed::BitPumpLSB;

namespace rawspeed_test {

template <>
const std::array<rawspeed::uchar8, 4> BitPumpTest<BitPumpLSB>::ones = {
    /* [Byte0 Byte1 Byte2 Byte3] */
    /* Byte: [Bit7 .. Bit0] */
    0b01001011, 0b10000100, 0b00100000, 0b11110000};

template <>
const std::array<rawspeed::uchar8, 4> BitPumpTest<BitPumpLSB>::invOnes = {
    0b00100101, 0b01000010, 0b00010000, 0b11111000};

INSTANTIATE_TYPED_TEST_CASE_P(LSB, BitPumpTest, BitPumpLSB);

} // namespace rawspeed_test
