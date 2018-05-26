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

#include "decompressors/HuffmanTable.h" // for HuffmanTableLUT, HuffmanTable
#include "common/Common.h"              // for uchar8
#include "io/BitPumpMSB.h"              // for BitPumpMSB, BitStream<>::fil...
#include "io/Buffer.h"                  // for Buffer, DataBuffer
#include "io/ByteStream.h"              // for ByteStream
#include "io/Endianness.h"              // for Endianness, Endianness::little
#include <array>                        // for array
#include <gtest/gtest.h>                // for Test, Message, TestPartResult
#include <initializer_list>             // for initializer_list<>::const_it...
#include <utility>                      // for move
#include <vector>                       // for vector

namespace rawspeed {
class RawDecoderException;
}

using rawspeed::BitPumpMSB;
using rawspeed::Buffer;
using rawspeed::ByteStream;
using rawspeed::DataBuffer;
using rawspeed::Endianness;
using rawspeed::HuffmanTable;
using rawspeed::uchar8;

namespace rawspeed_test {

auto genHT =
    [](std::initializer_list<uchar8>&& nCodesPerLength) -> HuffmanTable {
  HuffmanTable ht;
  std::vector<uchar8> v(nCodesPerLength.begin(), nCodesPerLength.end());
  v.resize(16);
  Buffer b(v.data(), v.size());
  ht.setNCodesPerLength(b);

  return ht;
};

auto genHTFull =
    [](std::initializer_list<uchar8>&& nCodesPerLength,
       std::initializer_list<uchar8>&& codeValues) -> HuffmanTable {
  auto ht = genHT(std::move(nCodesPerLength));
  std::vector<uchar8> v(codeValues.begin(), codeValues.end());
  Buffer b(v.data(), v.size());
  ht.setCodeValues(b);
  return ht;
};

TEST(HuffmanTableTest, DecodeLengthIdentityTest) {
  static const std::array<rawspeed::uchar8, 4> data{
      {0b01010101, 0b01010101, 0b01010101, 0b01010101}};
  const Buffer b(data.data(), data.size());
  const DataBuffer db(b, Endianness::little);
  const ByteStream bs(db);

  BitPumpMSB p(bs);

  auto ht = genHTFull({2}, {4, 8});
  ht.setup(false, false);

  for (int i = 0; i < 32; i += 2) {
    ASSERT_EQ(ht.decodeLength(p), 4);
    ASSERT_EQ(ht.decodeLength(p), 8);
  }
}

TEST(HuffmanTableTest, DecodeNextIdentityTest) {
  static const std::array<rawspeed::uchar8, 4> data{
      {0b00000000, 0b11010101, 0b01010101, 0b01111111}};
  const Buffer b(data.data(), data.size());
  const DataBuffer db(b, Endianness::little);
  const ByteStream bs(db);

  BitPumpMSB p(bs);

  auto ht = genHTFull({2}, {7, 7 + 8});
  ht.setup(true, false);

  ASSERT_EQ(ht.decodeNext(p), -127);
  ASSERT_EQ(ht.decodeNext(p), 21845);
  ASSERT_EQ(ht.decodeNext(p), 127);
}

TEST(HuffmanTableTest, DecodeLengthBadCodeTest) {
  static const std::array<rawspeed::uchar8, 4> data{{0b01000000}};
  const Buffer b(data.data(), data.size());
  const DataBuffer db(b, Endianness::little);
  const ByteStream bs(db);

  BitPumpMSB p(bs);

  auto ht = genHTFull({1}, {4});
  ht.setup(false, false);

  ASSERT_EQ(ht.decodeLength(p), 4);
  ASSERT_THROW(ht.decodeLength(p), rawspeed::RawDecoderException);
}

TEST(HuffmanTableTest, DecodeNextBadCodeTest) {
  static const std::array<rawspeed::uchar8, 4> data{{0b00100000}};
  const Buffer b(data.data(), data.size());
  const DataBuffer db(b, Endianness::little);
  const ByteStream bs(db);

  BitPumpMSB p(bs);

  auto ht = genHTFull({1}, {1});
  ht.setup(true, false);

  ASSERT_EQ(ht.decodeNext(p), -1);
  ASSERT_THROW(ht.decodeNext(p), rawspeed::RawDecoderException);
}

} // namespace rawspeed_test
