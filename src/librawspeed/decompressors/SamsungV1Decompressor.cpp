/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2010 Klaus Post
    Copyright (C) 2014-2015 Pedro Côrte-Real
    Copyright (C) 2017 Roman Lebedev

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

#include "decompressors/SamsungV1Decompressor.h"
#include "common/Array2DRef.h"            // for Array2DRef
#include "common/Common.h"                // for isIntN
#include "common/Point.h"                 // for iPoint2D
#include "common/RawImage.h"              // for RawImage, RawImageData
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "decompressors/HuffmanTable.h"   // for HuffmanTable
#include "io/BitPumpMSB.h"                // for BitPumpMSB
#include <array>                          // for array
#include <cassert>                        // for assert
#include <memory>                         // for allocator_traits<>::value_...
#include <vector>                         // for vector

namespace rawspeed {

struct SamsungV1Decompressor::encTableItem {
  uint8_t encLen;
  uint8_t diffLen;
};

SamsungV1Decompressor::SamsungV1Decompressor(RawImageData *image,
                                             const ByteStream& bs_, int bit)
    : AbstractSamsungDecompressor(image), bs(bs_) {
  if (mRaw->getCpp() != 1 || mRaw->getDataType() != RawImageType::UINT16 ||
      mRaw->getBpp() != sizeof(uint16_t))
    ThrowRDE("Unexpected component count / data type");

  if (bit != 12)
    ThrowRDE("Unexpected bit per pixel (%u)", bit);

  const uint32_t width = mRaw->dim.x;
  const uint32_t height = mRaw->dim.y;

  if (width == 0 || height == 0 || width % 32 != 0 || height % 2 != 0 ||
      width > 5664 || height > 3714)
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", width, height);
}

inline int32_t
SamsungV1Decompressor::samsungDiff(BitPumpMSB& pump,
                                   const std::vector<encTableItem>& tbl) {
  pump.fill(23); // That is the maximal number of bits we will need here.
  // We read 10 bits to index into our table
  uint32_t c = pump.peekBitsNoFill(10);
  // Skip the bits that were used to encode this case
  pump.skipBitsNoFill(tbl[c].encLen);
  // Read the number of bits the table tells me
  int32_t len = tbl[c].diffLen;
  if (len == 0)
    return 0;
  int32_t diff = pump.getBitsNoFill(len);
  // If the first bit is 0 we need to turn this into a negative number
  diff = HuffmanTable::extend(diff, len);
  return diff;
}

void SamsungV1Decompressor::decompress() const {
  // This format has a variable length encoding of how many bits are needed
  // to encode the difference between pixels, we use a table to process it
  // that has two values, the first the number of bits that were used to
  // encode, the second the number of bits that come after with the difference
  // The table has 14 entries because the difference can have between 0 (no
  // difference) and 13 bits (differences between 12 bits numbers can need 13)
  static const std::array<std::array<uint8_t, 2>, 14> tab = {{{3, 4},
                                                              {3, 7},
                                                              {2, 6},
                                                              {2, 5},
                                                              {4, 3},
                                                              {6, 0},
                                                              {7, 9},
                                                              {8, 10},
                                                              {9, 11},
                                                              {10, 12},
                                                              {10, 13},
                                                              {5, 1},
                                                              {4, 8},
                                                              {4, 2}}};
  std::vector<encTableItem> tbl(1024);

  // We generate a 1024 entry table (to be addressed by reading 10 bits) by
  // consecutively filling in 2^(10-N) positions where N is the variable number
  // of bits of the encoding. So for example 4 is encoded with 3 bits so the
  // first 2^(10-3)=128 positions are set with 3,4 so that any time we read 000
  // we know the next 4 bits are the difference. We read 10 bits because that is
  // the maximum number of bits used in the variable encoding (for the 12 and
  // 13 cases)
  uint32_t n = 0;
  for (auto i : tab) {
    for (int32_t c = 0; c < (1024 >> i[0]); c++) {
      tbl[n].encLen = i[0];
      tbl[n].diffLen = i[1];
      n++;
    }
  }

  auto *rawU16 = dynamic_cast<RawImageDataU16*>(mRaw);
  assert(rawU16);
  const Array2DRef<uint16_t> out(rawU16->getU16DataAsUncroppedArray2DRef());
  assert(out.width % 32 == 0 && "Should have even count of pixels per row.");
  assert(out.height % 2 == 0 && "Should have even row count.");
  BitPumpMSB pump(bs);
  for (int row = 0; row < out.height; row++) {
    std::array<int, 2> pred = {{}};
    if (row >= 2)
      pred = {out(row - 2, 0), out(row - 2, 1)};

    for (int col = 0; col < out.width; col++) {
      int32_t diff = samsungDiff(pump, tbl);
      pred[col & 1] += diff;

      int value = pred[col & 1];
      if (!isIntN(value, bits))
        ThrowRDE("decoded value out of bounds");
      out(row, col) = value;
    }
  }
}

} // namespace rawspeed
