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

#include "io/BitPumpMSB32.h" // for BitPumpMSB32
#include "io/BitPumpTest.h"  // for Pattern, (anonymous), GenOnesBE, Patterns
#include <array>             // for array
#include <cstdint>           // for uint32_t, uint8_t
#include <gtest/gtest.h>     // for INSTANTIATE_TYPED_TEST_CASE_P
#include <memory>            // for allocator

using rawspeed::BitPumpMSB32;

namespace rawspeed_test {

struct InvOnesTag;
struct OnesTag;

template <>
const std::array<uint8_t, 8> Pattern<BitPumpMSB32, OnesTag>::Data = {
    {/* [Byte3 Byte2 Byte1 Byte0] */
     /* Byte: [Bit0 .. Bit7] */
     0b00011111, 0b00001000, 0b01000010, 0b10100100}};
template <> uint32_t Pattern<BitPumpMSB32, OnesTag>::data(int index) {
  const auto set = GenOnesBE(1, 0);
  return set[index];
}

template <>
const std::array<uint8_t, 8> Pattern<BitPumpMSB32, InvOnesTag>::Data = {
    {0b00001111, 0b00000100, 0b00100001, 0b11010010}};
template <> uint32_t Pattern<BitPumpMSB32, InvOnesTag>::data(int index) {
  const auto set = GenOnesBE(0, -1);
  return set[index];
}

INSTANTIATE_TYPED_TEST_CASE_P(MSB32, BitPumpTest, Patterns<BitPumpMSB32>);

} // namespace rawspeed_test
