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

#include "bitstreams/BitStreamerJPEG.h"
#include "adt/Array1DRef.h"
#include "adt/Casts.h"
#include "bitstreams/BitStreamerTest.h"
#include "io/Buffer.h"
#include "io/ByteStream.h"
#include "io/Endianness.h"
#include <array>
#include <cstdint>
#include <gtest/gtest.h>

using rawspeed::BitStreamerJPEG;
using rawspeed::Buffer;
using rawspeed::ByteStream;
using rawspeed::DataBuffer;
using rawspeed::Endianness;

namespace rawspeed_test {

struct InvOnesTag;
struct OnesTag;
struct SaturatedTag;

template <>
const std::array<uint8_t, 8> Pattern<BitStreamerJPEG, OnesTag>::Data = {
    {/* [Byte0 Byte1 Byte2 Byte3] */
     /* Byte: [Bit0 .. Bit7] */
     0b10100100, 0b01000010, 0b00001000, 0b00011111}};
template <> uint32_t Pattern<BitStreamerJPEG, OnesTag>::data(int index) {
  const auto set = GenOnesBE(1, 0);
  return set[index];
}

template <>
const std::array<uint8_t, 8> Pattern<BitStreamerJPEG, InvOnesTag>::Data = {
    {0b11010010, 0b00100001, 0b00000100, 0b00001111}};
template <> uint32_t Pattern<BitStreamerJPEG, InvOnesTag>::data(int index) {
  const auto set = GenOnesBE(0, -1);
  return set[index];
}

// If 0xFF0x00 byte sequence is found, it is just 0xFF, i.e. 0x00 is ignored.
// So if we want 0xFF, we need to append 0x00 byte
template <>
const std::array<uint8_t, 8> Pattern<BitStreamerJPEG, SaturatedTag>::Data{
    {uint8_t(~0U), 0, uint8_t(~0U), 0, uint8_t(~0U), 0, uint8_t(~0U), 0}};

INSTANTIATE_TYPED_TEST_SUITE_P(JPEG, BitStreamerTest,
                               Patterns<BitStreamerJPEG>);

TEST(BitStreamerJPEGTest, 0xFF0x00Is0xFFTest) {
  // If 0xFF0x00 byte sequence is found, it is just 0xFF, i.e. 0x00 is ignored.
  static const std::array<uint8_t, 2 + 8> data{
      {0xFF, 0x00, 0b10100100, 0b01000010, 0b00001000, 0b00011111}};

  const rawspeed::Array1DRef<const uint8_t> input(
      data.data(), rawspeed::implicit_cast<int>(data.size()));

  BitStreamerJPEG p(input);

  ASSERT_EQ(p.getBits(8), 0xFF);

  for (int len = 1; len <= 7; len++)
    ASSERT_EQ(p.getBits(len), 1) << "     Where len: " << len;
}

TEST(BitStreamerJPEGTest, 0xFF0xXXIsTheEndTest) {
  // If 0xFF0xXX byte sequence is found, where XX != 0, then it is the end.
  for (uint8_t end = 0x01; end < 0xFF; end++) {
    static const std::array<uint8_t, 2 + 8> data{
        {0xFF, end, 0xFF, 0xFF, 0xFF, 0xFF}};

    const rawspeed::Array1DRef<const uint8_t> input(
        data.data(), rawspeed::implicit_cast<int>(data.size()));

    BitStreamerJPEG p(input);

    for (int cnt = 0; cnt <= 64 + 32 - 1; cnt++)
      ASSERT_EQ(p.getBits(1), 0);
  }
}

} // namespace rawspeed_test
