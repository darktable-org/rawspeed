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
#include "common/Common.h"                // for uint32, ushort16, int32
#include "common/Point.h"                 // for iPoint2D
#include "common/RawImage.h"              // for RawImage, RawImageData
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "decompressors/HuffmanTable.h"   // for HuffmanTable
#include "io/BitPumpMSB.h"                // for BitPumpMSB
#include "tiff/TiffEntry.h"               // for TiffEntry
#include "tiff/TiffIFD.h"                 // for TiffIFD
#include "tiff/TiffTag.h"                 // for TiffTag::IMAGELENGTH, Tiff...
#include <memory>                         // for allocator_traits<>::value_...
#include <vector>                         // for vector

namespace rawspeed {

struct SamsungV1Decompressor::encTableItem {
  uchar8 encLen;
  uchar8 diffLen;
};

int32 SamsungV1Decompressor::samsungDiff(BitPumpMSB* pump,
                                         const std::vector<encTableItem>& tbl) {
  // We read 10 bits to index into our table
  uint32 c = pump->peekBits(10);
  // Skip the bits that were used to encode this case
  pump->getBits(tbl[c].encLen);
  // Read the number of bits the table tells me
  int32 len = tbl[c].diffLen;
  int32 diff = pump->getBits(len);

  // If the first bit is 0 we need to turn this into a negative number
  diff = len != 0 ? HuffmanTable::signExtended(diff, len) : diff;

  return diff;
}

void SamsungV1Decompressor::decompress() {
  uint32 width = raw->getEntry(IMAGEWIDTH)->getU32();
  uint32 height = raw->getEntry(IMAGELENGTH)->getU32();

  if (width == 0 || height == 0 || width > 5664 || height > 3714)
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", width, height);

  mRaw->dim = iPoint2D(width, height);
  mRaw->createData();

  // This format has a variable length encoding of how many bits are needed
  // to encode the difference between pixels, we use a table to process it
  // that has two values, the first the number of bits that were used to
  // encode, the second the number of bits that come after with the difference
  // The table has 14 entries because the difference can have between 0 (no
  // difference) and 13 bits (differences between 12 bits numbers can need 13)
  const uchar8 tab[14][2] = {{3, 4},   {3, 7}, {2, 6},  {2, 5},  {4, 3},
                             {6, 0},   {7, 9}, {8, 10}, {9, 11}, {10, 12},
                             {10, 13}, {5, 1}, {4, 8},  {4, 2}};
  std::vector<encTableItem> tbl(1024);
  ushort16 vpred[2][2] = {{0, 0}, {0, 0}};
  ushort16 hpred[2];

  // We generate a 1024 entry table (to be addressed by reading 10 bits) by
  // consecutively filling in 2^(10-N) positions where N is the variable number
  // of bits of the encoding. So for example 4 is encoded with 3 bits so the
  // first 2^(10-3)=128 positions are set with 3,4 so that any time we read 000
  // we know the next 4 bits are the difference. We read 10 bits because that is
  // the maximum number of bits used in the variable encoding (for the 12 and
  // 13 cases)
  uint32 n = 0;
  for (auto i : tab) {
    for (int32 c = 0; c < (1024 >> i[0]); c++) {
      tbl[n].encLen = i[0];
      tbl[n].diffLen = i[1];
      n++;
    }
  }

  BitPumpMSB pump(*bs);
  for (uint32 y = 0; y < height; y++) {
    auto* img = reinterpret_cast<ushort16*>(mRaw->getData(0, y));
    for (uint32 x = 0; x < width; x++) {
      int32 diff = samsungDiff(&pump, tbl);
      if (x < 2)
        hpred[x] = vpred[y & 1][x] += diff;
      else
        hpred[x & 1] += diff;
      img[x] = hpred[x & 1];
      if (img[x] >> bits)
        ThrowRDE("decoded value out of bounds at %d:%d", x, y);
    }
  }
}

} // namespace rawspeed
