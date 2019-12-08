/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post

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

#include "decompressors/PentaxDecompressor.h"
#include "common/Common.h"                // for uint32_t, uint8_t, uint16_t
#include "common/Point.h"                 // for iPoint2D
#include "common/RawImage.h"              // for RawImage, RawImageData
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "decompressors/HuffmanTable.h"   // for HuffmanTable
#include "io/BitPumpMSB.h"                // for BitPumpMSB, BitStream<>::f...
#include "io/Buffer.h"                    // for Buffer
#include "io/ByteStream.h"                // for ByteStream
#include <cassert>                        // for assert
#include <vector>                         // for vector

namespace rawspeed {

// 16 entries of codes per bit length
// 13 entries of code values
const std::array<std::array<std::array<uint8_t, 16>, 2>, 1>
    PentaxDecompressor::pentax_tree = {{
        {{{0, 2, 3, 1, 1, 1, 1, 1, 1, 2, 0, 0, 0, 0, 0, 0},
          {3, 4, 2, 5, 1, 6, 0, 7, 8, 9, 10, 11, 12}}},
    }};

PentaxDecompressor::PentaxDecompressor(const RawImage& img,
                                       ByteStream* metaData)
    : mRaw(img), ht(SetupHuffmanTable(metaData)) {
  if (mRaw->getCpp() != 1 || mRaw->getDataType() != TYPE_USHORT16 ||
      mRaw->getBpp() != 2)
    ThrowRDE("Unexpected component count / data type");

  if (!mRaw->dim.x || !mRaw->dim.y || mRaw->dim.x % 2 != 0 ||
      mRaw->dim.x > 8384 || mRaw->dim.y > 6208) {
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", mRaw->dim.x,
             mRaw->dim.y);
  }
}

HuffmanTable PentaxDecompressor::SetupHuffmanTable_Legacy() {
  HuffmanTable ht;

  /* Initialize with legacy data */
  auto nCodes = ht.setNCodesPerLength(Buffer(pentax_tree[0][0].data(), 16));
  assert(nCodes == 13); // see pentax_tree definition
  ht.setCodeValues(Buffer(pentax_tree[0][1].data(), nCodes));

  return ht;
}

HuffmanTable PentaxDecompressor::SetupHuffmanTable_Modern(ByteStream stream) {
  HuffmanTable ht;

  const uint32_t depth = stream.getU16() + 12;
  if (depth > 15)
    ThrowRDE("Depth of huffman table is too great (%u).", depth);

  stream.skipBytes(12);

  std::array<uint32_t, 16> v0;
  std::array<uint32_t, 16> v1;
  for (uint32_t i = 0; i < depth; i++)
    v0[i] = stream.getU16();
  for (uint32_t i = 0; i < depth; i++) {
    v1[i] = stream.getByte();

    if (v1[i] == 0 || v1[i] > 12)
      ThrowRDE("Data corrupt: v1[%i]=%i, expected [1..12]", depth, v1[i]);
  }

  std::vector<uint8_t> nCodesPerLength;
  nCodesPerLength.resize(17);

  std::array<uint32_t, 16> v2;
  /* Calculate codes and store bitcounts */
  for (uint32_t c = 0; c < depth; c++) {
    v2[c] = extractHighBits(v0[c], v1[c], /*effectiveBitwidth=*/12);
    nCodesPerLength.at(v1[c])++;
  }

  assert(nCodesPerLength.size() == 17);
  assert(nCodesPerLength[0] == 0);
  auto nCodes = ht.setNCodesPerLength(Buffer(&nCodesPerLength[1], 16));
  assert(nCodes == depth);

  std::vector<uint8_t> codeValues;
  codeValues.reserve(nCodes);

  /* Find smallest */
  for (uint32_t i = 0; i < depth; i++) {
    uint32_t sm_val = 0xfffffff;
    uint32_t sm_num = 0xff;
    for (uint32_t j = 0; j < depth; j++) {
      if (v2[j] <= sm_val) {
        sm_num = j;
        sm_val = v2[j];
      }
    }
    codeValues.push_back(sm_num);
    v2[sm_num] = 0xffffffff;
  }

  assert(codeValues.size() == nCodes);
  ht.setCodeValues(Buffer(codeValues.data(), nCodes));

  return ht;
}

HuffmanTable PentaxDecompressor::SetupHuffmanTable(ByteStream* metaData) {
  HuffmanTable ht;

  if (metaData)
    ht = SetupHuffmanTable_Modern(*metaData);
  else
    ht = SetupHuffmanTable_Legacy();

  ht.setup(true, false);

  return ht;
}

void PentaxDecompressor::decompress(const ByteStream& data) const {
  const Array2DRef<uint16_t> out(mRaw->getU16DataAsUncroppedArray2DRef());

  assert(out.height > 0);
  assert(out.width > 0);
  assert(out.width % 2 == 0);

  BitPumpMSB bs(data);
  for (int row = 0; row < out.height; row++) {
    std::array<int, 2> pred = {{}};
    if (row >= 2)
      pred = {out(row - 2, 0), out(row - 2, 1)};

    for (int col = 0; col < out.width; col++) {
      pred[col & 1] += ht.decodeDifference(bs);
      int value = pred[col & 1];
      if (!isIntN(value, 16))
        ThrowRDE("decoded value out of bounds at %d:%d", col, row);
      out(row, col) = value;
    }
  }
}

} // namespace rawspeed
