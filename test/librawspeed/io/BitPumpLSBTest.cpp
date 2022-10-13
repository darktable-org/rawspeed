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
#include "io/BitPumpTest.h" // for Pattern, (anonymous), GenOnesLE, BitPump...
#include <array>            // for array
#include <cstdint>          // for uint32_t, uint8_t
#include <gtest/gtest.h>    // for Types, INSTANTIATE_TYPED_TEST_CASE_P

using rawspeed::BitPumpLSB;

namespace rawspeed_test {

struct InvOnesTag;
struct OnesTag;

template <>
const std::array<uint8_t, 8> Pattern<BitPumpLSB, OnesTag>::Data = {
    {/* [Byte0 Byte1 Byte2 Byte3] */
     /* Byte: [Bit7 .. Bit0] */
     0b01001011, 0b10000100, 0b00100000, 0b11110000}};
template <> uint32_t Pattern<BitPumpLSB, OnesTag>::data(int index) {
  const auto set = GenOnesLE(0, -1);
  return set[index];
}

template <>
const std::array<uint8_t, 8> Pattern<BitPumpLSB, InvOnesTag>::Data = {
    {0b00100101, 0b01000010, 0b00010000, 0b11111000}};
template <> uint32_t Pattern<BitPumpLSB, InvOnesTag>::data(int index) {
  const auto set = GenOnesLE(1, 0);
  return set[index];
}

INSTANTIATE_TYPED_TEST_CASE_P(LSB, BitPumpTest, Patterns<BitPumpLSB>);

} // namespace rawspeed_test
