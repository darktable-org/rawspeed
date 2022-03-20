/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2014 Pedro Côrte-Real
    Copyright (C) 2015-2018 Roman Lebedev

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

#include "decompressors/CrwDecompressor.h"
#include "common/Array2DRef.h"            // for Array2DRef
#include "common/Common.h"                // for isIntN
#include "common/Point.h"                 // for iPoint2D
#include "common/RawImage.h"              // for RawImage, RawImageData
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "decompressors/HuffmanTable.h"   // for HuffmanTable, HuffmanTableLUT
#include "io/BitPumpJPEG.h"               // for BitPumpJPEG, BitStream<>::...
#include "io/Buffer.h"                    // for Buffer
#include "io/ByteStream.h"                // for ByteStream
#include <array>                          // for array
#include <cassert>                        // for assert
#include <cstdint>                        // for uint8_t, int16_t, uint16_t

using std::array;

namespace rawspeed {

CrwDecompressor::CrwDecompressor(RawImageData* img, uint32_t dec_table,
                                 bool lowbits_, ByteStream rawData)
    : mRaw(img), lowbits(lowbits_) {
  if (mRaw->getCpp() != 1 || mRaw->getDataType() != RawImageType::UINT16 ||
      mRaw->getBpp() != sizeof(uint16_t))
    ThrowRDE("Unexpected component count / data type");

  const uint32_t width = mRaw->dim.x;
  const uint32_t height = mRaw->dim.y;

  if (width == 0 || height == 0 || width % 4 != 0 || width > 4104 ||
      height > 3048 || (height * width) % 64 != 0)
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", width, height);

  if (lowbits) {
    // If there are low bits, the first part (size is calculable) is low bits
    // Each block is 4 pairs of 2 bits, so we have 1 block per 4 pixels
    const unsigned lBlocks = 1 * height * width / 4;
    assert(lBlocks > 0);
    lowbitInput = rawData.getStream(lBlocks);
  }

  // We always ignore next 514 bytes of 'padding'. No idea what is in there.
  rawData.skipBytes(514);

  // Rest is the high bits.
  rawInput = rawData.getStream(rawData.getRemainSize());

  mHuff = initHuffTables(dec_table);
}

HuffmanTable CrwDecompressor::makeDecoder(const uint8_t* ncpl,
                                          const uint8_t* values) {
  assert(ncpl);

  HuffmanTable ht;
  auto count = ht.setNCodesPerLength(Buffer(ncpl, 16));
  ht.setCodeValues(Buffer(values, count));
  ht.setup(/*fullDecode_=*/false, false);

  return ht;
}

CrwDecompressor::crw_hts CrwDecompressor::initHuffTables(uint32_t table) {
  if (table > 2)
    ThrowRDE("Wrong table number: %u", table);

  // NCodesPerLength
  static const std::array<std::array<uint8_t, 16>, 3> first_tree_ncpl = {{
      {0, 1, 4, 2, 3, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {0, 2, 2, 3, 1, 1, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0},
      {0, 0, 6, 3, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  }};

  static const std::array<std::array<uint8_t, 13>, 3> first_tree_codevalues = {{
      {0x04, 0x03, 0x05, 0x06, 0x02, 0x07, 0x01, 0x08, 0x09, 0x00, 0x0a, 0x0b,
       0xff},
      {0x03, 0x02, 0x04, 0x01, 0x05, 0x00, 0x06, 0x07, 0x09, 0x08, 0x0a, 0x0b,
       0xff},
      {0x06, 0x05, 0x07, 0x04, 0x08, 0x03, 0x09, 0x02, 0x00, 0x0a, 0x01, 0x0b,
       0xff},
  }};

  // NCodesPerLength
  static const std::array<std::array<uint8_t, 16>, 3> second_tree_ncpl = {{
      {0, 2, 2, 2, 1, 4, 2, 1, 2, 5, 1, 1, 0, 0, 0, 139},
      {0, 2, 2, 1, 4, 1, 4, 1, 3, 3, 1, 0, 0, 0, 0, 140},
      {0, 0, 6, 2, 1, 3, 3, 2, 5, 1, 2, 2, 8, 10, 0, 117},
  }};

  static const std::array<std::array<uint8_t, 164>, 3> second_tree_codevalues =
      {{{0x03, 0x04, 0x02, 0x05, 0x01, 0x06, 0x07, 0x08, 0x12, 0x13, 0x11, 0x14,
         0x09, 0x15, 0x22, 0x00, 0x21, 0x16, 0x0a, 0xf0, 0x23, 0x17, 0x24, 0x31,
         0x32, 0x18, 0x19, 0x33, 0x25, 0x41, 0x34, 0x42, 0x35, 0x51, 0x36, 0x37,
         0x38, 0x29, 0x79, 0x26, 0x1a, 0x39, 0x56, 0x57, 0x28, 0x27, 0x52, 0x55,
         0x58, 0x43, 0x76, 0x59, 0x77, 0x54, 0x61, 0xf9, 0x71, 0x78, 0x75, 0x96,
         0x97, 0x49, 0xb7, 0x53, 0xd7, 0x74, 0xb6, 0x98, 0x47, 0x48, 0x95, 0x69,
         0x99, 0x91, 0xfa, 0xb8, 0x68, 0xb5, 0xb9, 0xd6, 0xf7, 0xd8, 0x67, 0x46,
         0x45, 0x94, 0x89, 0xf8, 0x81, 0xd5, 0xf6, 0xb4, 0x88, 0xb1, 0x2a, 0x44,
         0x72, 0xd9, 0x87, 0x66, 0xd4, 0xf5, 0x3a, 0xa7, 0x73, 0xa9, 0xa8, 0x86,
         0x62, 0xc7, 0x65, 0xc8, 0xc9, 0xa1, 0xf4, 0xd1, 0xe9, 0x5a, 0x92, 0x85,
         0xa6, 0xe7, 0x93, 0xe8, 0xc1, 0xc6, 0x7a, 0x64, 0xe1, 0x4a, 0x6a, 0xe6,
         0xb3, 0xf1, 0xd3, 0xa5, 0x8a, 0xb2, 0x9a, 0xba, 0x84, 0xa4, 0x63, 0xe5,
         0xc5, 0xf3, 0xd2, 0xc4, 0x82, 0xaa, 0xda, 0xe4, 0xf2, 0xca, 0x83, 0xa3,
         0xa2, 0xc3, 0xea, 0xc2, 0xe2, 0xe3, 0xff, 0xff},
        {0x02, 0x03, 0x01, 0x04, 0x05, 0x12, 0x11, 0x06, 0x13, 0x07, 0x08, 0x14,
         0x22, 0x09, 0x21, 0x00, 0x23, 0x15, 0x31, 0x32, 0x0a, 0x16, 0xf0, 0x24,
         0x33, 0x41, 0x42, 0x19, 0x17, 0x25, 0x18, 0x51, 0x34, 0x43, 0x52, 0x29,
         0x35, 0x61, 0x39, 0x71, 0x62, 0x36, 0x53, 0x26, 0x38, 0x1a, 0x37, 0x81,
         0x27, 0x91, 0x79, 0x55, 0x45, 0x28, 0x72, 0x59, 0xa1, 0xb1, 0x44, 0x69,
         0x54, 0x58, 0xd1, 0xfa, 0x57, 0xe1, 0xf1, 0xb9, 0x49, 0x47, 0x63, 0x6a,
         0xf9, 0x56, 0x46, 0xa8, 0x2a, 0x4a, 0x78, 0x99, 0x3a, 0x75, 0x74, 0x86,
         0x65, 0xc1, 0x76, 0xb6, 0x96, 0xd6, 0x89, 0x85, 0xc9, 0xf5, 0x95, 0xb4,
         0xc7, 0xf7, 0x8a, 0x97, 0xb8, 0x73, 0xb7, 0xd8, 0xd9, 0x87, 0xa7, 0x7a,
         0x48, 0x82, 0x84, 0xea, 0xf4, 0xa6, 0xc5, 0x5a, 0x94, 0xa4, 0xc6, 0x92,
         0xc3, 0x68, 0xb5, 0xc8, 0xe4, 0xe5, 0xe6, 0xe9, 0xa2, 0xa3, 0xe3, 0xc2,
         0x66, 0x67, 0x93, 0xaa, 0xd4, 0xd5, 0xe7, 0xf8, 0x88, 0x9a, 0xd7, 0x77,
         0xc4, 0x64, 0xe2, 0x98, 0xa5, 0xca, 0xda, 0xe8, 0xf3, 0xf6, 0xa9, 0xb2,
         0xb3, 0xf2, 0xd2, 0x83, 0xba, 0xd3, 0xff, 0xff},
        {0x04, 0x05, 0x03, 0x06, 0x02, 0x07, 0x01, 0x08, 0x09, 0x12, 0x13, 0x14,
         0x11, 0x15, 0x0a, 0x16, 0x17, 0xf0, 0x00, 0x22, 0x21, 0x18, 0x23, 0x19,
         0x24, 0x32, 0x31, 0x25, 0x33, 0x38, 0x37, 0x34, 0x35, 0x36, 0x39, 0x79,
         0x57, 0x58, 0x59, 0x28, 0x56, 0x78, 0x27, 0x41, 0x29, 0x77, 0x26, 0x42,
         0x76, 0x99, 0x1a, 0x55, 0x98, 0x97, 0xf9, 0x48, 0x54, 0x96, 0x89, 0x47,
         0xb7, 0x49, 0xfa, 0x75, 0x68, 0xb6, 0x67, 0x69, 0xb9, 0xb8, 0xd8, 0x52,
         0xd7, 0x88, 0xb5, 0x74, 0x51, 0x46, 0xd9, 0xf8, 0x3a, 0xd6, 0x87, 0x45,
         0x7a, 0x95, 0xd5, 0xf6, 0x86, 0xb4, 0xa9, 0x94, 0x53, 0x2a, 0xa8, 0x43,
         0xf5, 0xf7, 0xd4, 0x66, 0xa7, 0x5a, 0x44, 0x8a, 0xc9, 0xe8, 0xc8, 0xe7,
         0x9a, 0x6a, 0x73, 0x4a, 0x61, 0xc7, 0xf4, 0xc6, 0x65, 0xe9, 0x72, 0xe6,
         0x71, 0x91, 0x93, 0xa6, 0xda, 0x92, 0x85, 0x62, 0xf3, 0xc5, 0xb2, 0xa4,
         0x84, 0xba, 0x64, 0xa5, 0xb3, 0xd2, 0x81, 0xe5, 0xd3, 0xaa, 0xc4, 0xca,
         0xf2, 0xb1, 0xe4, 0xd1, 0x83, 0x63, 0xea, 0xc3, 0xe2, 0x82, 0xf1, 0xa3,
         0xc2, 0xa1, 0xc1, 0xe3, 0xa2, 0xe1, 0xff, 0xff}}};

  std::array<HuffmanTable, 2> mHuff = {
      {makeDecoder(first_tree_ncpl[table].data(),
                   first_tree_codevalues[table].data()),
       makeDecoder(second_tree_ncpl[table].data(),
                   second_tree_codevalues[table].data())}};

  return mHuff;
}

inline void CrwDecompressor::decodeBlock(std::array<int16_t, 64>* diffBuf,
                                         const crw_hts& mHuff,
                                         BitPumpJPEG& bs) {
  assert(diffBuf);

  // decode the block
  for (int i = 0; i < 64;) {
    bs.fill(32);

    const uint8_t codeValue = mHuff[i > 0].decodeCodeValue(bs);
    const int len = codeValue & 0b1111;
    const int index = codeValue >> 4;
    assert(len >= 0 && index >= 0);

    if (len == 0 && index == 0 && i)
      break;

    if (len == 0xf && index == 0xf) {
      ++i;
      continue;
    }

    i += index;

    if (len == 0) {
      ++i;
      continue;
    }

    int diff = bs.getBitsNoFill(len);

    if (i >= 64)
      break;

    diff = HuffmanTable::extend(diff, len);

    (*diffBuf)[i] = diff;
    ++i;
  }
}

// FIXME: this function is horrible.
void CrwDecompressor::decompress() {
  const Array2DRef<uint16_t> out(mRaw->getU16DataAsUncroppedArray2DRef());
  assert(out.width > 0);
  assert(out.width % 4 == 0);
  assert(out.height > 0);

  {
    // Each block encodes 64 pixels

    assert((out.height * out.width) % 64 == 0);
    const unsigned hBlocks = out.height * out.width / 64;
    assert(hBlocks > 0);

    BitPumpJPEG bs(rawInput);

    int carry = 0;
    std::array<int, 2> base = {512, 512}; // starting predictors

    int row = 0;
    int col = 0;

    for (unsigned block = 0; block < hBlocks; block++) {
      array<int16_t, 64> diffBuf = {{}};
      decodeBlock(&diffBuf, mHuff, bs);

      // predict and output the block

      diffBuf[0] += carry;
      carry = diffBuf[0];

      for (uint32_t k = 0; k < 64; ++k) {
        if (col == out.width) {
          // new line. sadly, does not always happen when k == 0.
          col = 0;
          row++;
          base = {512, 512}; // reinit.
        }

        base[k & 1] += diffBuf[k];

        if (!isIntN(base[k & 1], 10))
          ThrowRDE("Error decompressing");

        out(row, col) = base[k & 1];
        ++col;
      }
    }
    assert(row == (out.height - 1));
    assert(col == out.width);
  }

  // Add the uncompressed 2 low bits to the decoded 8 high bits
  if (lowbits) {
    for (int row = 0; row < out.height; row++) {
      for (int col = 0; col < out.width; /* NOTE: col += 4 */) {
        const uint8_t c = lowbitInput.getByte();
        // LSB-packed: p3 << 6 | p2 << 4 | p1 << 2 | p0 << 0

        // We have read 8 bits, which is 4 pairs of 2 bits. So process 4 pixels.
        for (uint32_t p = 0; p < 4; ++p, ++col) {
          uint16_t& pixel = out(row, col);

          uint16_t low = (c >> (2 * p)) & 0b11;
          uint16_t val = (pixel << 2) | low;

          if (out.width == 2672 && val < 512)
            val += 2; // No idea why this is needed

          pixel = val;
        }
      }
    }
  }
}

} // namespace rawspeed
