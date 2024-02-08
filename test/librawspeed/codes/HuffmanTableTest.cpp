/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2018 Roman Lebedev

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

#include "adt/Array1DRef.h"
#include "bitstreams/BitStreamer.h"
#include "bitstreams/BitStreamerMSB.h"
#include "codes/HuffmanCode.h"
#include "codes/PrefixCodeDecoder.h"
#include "io/Buffer.h"
#include "io/ByteStream.h"
#include "io/Endianness.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <initializer_list>
#include <utility>
#include <vector>
#include <gtest/gtest.h>

namespace rawspeed {
class RawDecoderException;
}

using rawspeed::BitStreamerMSB;
using rawspeed::Buffer;
using rawspeed::ByteStream;
using rawspeed::DataBuffer;
using rawspeed::Endianness;
using rawspeed::PrefixCodeDecoder;

namespace rawspeed_test {

auto genHTFull =
    [](std::initializer_list<uint8_t>&& nCodesPerLength,
       std::initializer_list<uint8_t>&& codeValues) -> PrefixCodeDecoder<> {
  rawspeed::HuffmanCode<rawspeed::BaselineCodeTag> hc;

  std::vector<uint8_t> lv(nCodesPerLength.begin(), nCodesPerLength.end());
  lv.resize(16);
  Buffer lb(lv.data(), lv.size());
  hc.setNCodesPerLength(lb);

  std::vector<uint8_t> cv(codeValues.begin(), codeValues.end());
  rawspeed::Array1DRef<uint8_t> cb(cv.data(), cv.size());
  hc.setCodeValues(cb);

  auto code = hc.operator rawspeed::PrefixCode<rawspeed::BaselineCodeTag>();
  PrefixCodeDecoder<> ht(std::move(code));
  return ht;
};

TEST(PrefixCodeDecoderTest, decodeCodeValueIdentityTest) {
  static const std::array<uint8_t, 4> data{
      {0b01010101, 0b01010101, 0b01010101, 0b01010101}};
  const Buffer b(data.data(), data.size());
  const DataBuffer db(b, Endianness::little);
  const ByteStream bs(db);

  BitStreamerMSB p(bs);

  auto ht = genHTFull({2}, {4, 8});
  ht.setup(false, false);

  for (int i = 0; i < 32; i += 2) {
    ASSERT_EQ(ht.decodeCodeValue(p), 4);
    ASSERT_EQ(ht.decodeCodeValue(p), 8);
  }
}

TEST(PrefixCodeDecoderTest, decodeDifferenceIdentityTest) {
  static const std::array<uint8_t, 4> data{
      {0b00000000, 0b11010101, 0b01010101, 0b01111111}};
  const Buffer b(data.data(), data.size());
  const DataBuffer db(b, Endianness::little);
  const ByteStream bs(db);

  BitStreamerMSB p(bs);

  auto ht = genHTFull({2}, {7, 7 + 8});
  ht.setup(true, false);

  ASSERT_EQ(ht.decodeDifference(p), -127);
  ASSERT_EQ(ht.decodeDifference(p), 21845);
  ASSERT_EQ(ht.decodeDifference(p), 127);
}

TEST(PrefixCodeDecoderTest, decodeCodeValueBadCodeTest) {
  static const std::array<uint8_t, 4> data{{0b01000000}};
  const Buffer b(data.data(), data.size());
  const DataBuffer db(b, Endianness::little);
  const ByteStream bs(db);

  BitStreamerMSB p(bs);

  auto ht = genHTFull({1}, {4});
  ht.setup(false, false);

  ASSERT_EQ(ht.decodeCodeValue(p), 4);
  ASSERT_THROW(ht.decodeCodeValue(p), rawspeed::RawDecoderException);
}

TEST(PrefixCodeDecoderTest, decodeDifferenceBadCodeTest) {
  static const std::array<uint8_t, 4> data{{0b00100000}};
  const Buffer b(data.data(), data.size());
  const DataBuffer db(b, Endianness::little);
  const ByteStream bs(db);

  BitStreamerMSB p(bs);

  auto ht = genHTFull({1}, {1});
  ht.setup(true, false);

  ASSERT_EQ(ht.decodeDifference(p), -1);
  ASSERT_THROW(ht.decodeDifference(p), rawspeed::RawDecoderException);
}

} // namespace rawspeed_test
