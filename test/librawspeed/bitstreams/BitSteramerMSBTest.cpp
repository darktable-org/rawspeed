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

#include "bitstreams/BitStreamerMSB.h"
#include "bitstreams/BitStreamerTest.h"
#include <array>
#include <cstdint>
#include <gtest/gtest.h>

using rawspeed::BitStreamerMSB;

namespace rawspeed_test {

struct InvOnesTag;
struct OnesTag;

template <>
const std::array<uint8_t, 8> Pattern<BitStreamerMSB, OnesTag>::Data = {
    {/* [Byte0 Byte1 Byte2 Byte3] */
     /* Byte: [Bit0 .. Bit7] */
     0b10100100, 0b01000010, 0b00001000, 0b00011111}};
template <> uint32_t Pattern<BitStreamerMSB, OnesTag>::data(int index) {
  const auto set = GenOnesBE(1, 0);
  return set[index];
}

template <>
const std::array<uint8_t, 8> Pattern<BitStreamerMSB, InvOnesTag>::Data = {
    {0b11010010, 0b00100001, 0b00000100, 0b00001111}};
template <> uint32_t Pattern<BitStreamerMSB, InvOnesTag>::data(int index) {
  const auto set = GenOnesBE(0, -1);
  return set[index];
}

INSTANTIATE_TYPED_TEST_SUITE_P(MSB, BitStreamerTest, Patterns<BitStreamerMSB>);

} // namespace rawspeed_test
