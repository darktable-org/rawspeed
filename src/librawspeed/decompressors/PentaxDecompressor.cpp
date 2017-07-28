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
#include "common/Common.h"                // for uint32, uchar8, ushort16
#include "common/Point.h"                 // for iPoint2D
#include "common/RawImage.h"              // for RawImage, RawImageData
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "decompressors/HuffmanTable.h"   // for HuffmanTable
#include "io/BitPumpMSB.h"                // for BitPumpMSB, BitStream<>::f...
#include "io/Buffer.h"                    // for Buffer
#include "io/ByteStream.h"                // for ByteStream
#include "tiff/TiffEntry.h"               // for TiffEntry, ::TIFF_UNDEFINED
#include "tiff/TiffIFD.h"                 // for TiffIFD
#include "tiff/TiffTag.h"                 // for TiffTag
#include <cassert>                        // for assert
#include <vector>                         // for vector, allocator

namespace rawspeed {

// 16 entries of codes per bit length
// 13 entries of code values
const uchar8 PentaxDecompressor::pentax_tree[][2][16] = {
    {{0, 2, 3, 1, 1, 1, 1, 1, 1, 2, 0, 0, 0, 0, 0, 0},
     {3, 4, 2, 5, 1, 6, 0, 7, 8, 9, 10, 11, 12}},
};

void PentaxDecompressor::decompress(const RawImage& mRaw, ByteStream&& data,
                                    TiffIFD* root) {

  HuffmanTable ht;

  /* Attempt to read huffman table, if found in makernote */
  if (root->hasEntryRecursive(static_cast<TiffTag>(0x220))) {
    TiffEntry* t = root->getEntryRecursive(static_cast<TiffTag>(0x220));
    if (t->type == TIFF_UNDEFINED) {

      ByteStream stream = t->getData();

      uint32 depth = (stream.getU16()+12)&0xf;
      stream.skipBytes(12);
      uint32 v0[16];
      uint32 v1[16];
      uint32 v2[16];
      for (uint32 i = 0; i < depth; i++)
         v0[i] = stream.getU16();

      for (uint32 i = 0; i < depth; i++)
        v1[i] = stream.getByte();

      std::vector<uchar8> nCodesPerLength;
      nCodesPerLength.resize(17);

      /* Calculate codes and store bitcounts */
      for (uint32 c = 0; c < depth; c++) {
        v2[c] = v0[c]>>(12-v1[c]);
        nCodesPerLength.at(v1[c])++;
      }

      assert(nCodesPerLength.size() == 17);
      assert(nCodesPerLength[0] == 0);
      auto nCodes = ht.setNCodesPerLength(Buffer(&nCodesPerLength[1], 16));
      assert(nCodes == depth);

      std::vector<uchar8> codeValues;
      codeValues.reserve(nCodes);

      /* Find smallest */
      for (uint32 i = 0; i < depth; i++) {
        uint32 sm_val = 0xfffffff;
        uint32 sm_num = 0xff;
        for (uint32 j = 0; j < depth; j++) {
          if(v2[j]<=sm_val) {
            sm_num = j;
            sm_val = v2[j];
          }
        }
        codeValues.push_back(sm_num);
        v2[sm_num]=0xffffffff;
      }

      assert(codeValues.size() == nCodes);
      ht.setCodeValues(Buffer(codeValues.data(), nCodes));
    } else {
      ThrowRDE("Unknown Huffman table type.");
    }
  } else {
    /* Initialize with legacy data */
    auto nCodes = ht.setNCodesPerLength(Buffer(pentax_tree[0][0], 16));
    assert(nCodes == 13); // see pentax_tree definition
    ht.setCodeValues(Buffer(pentax_tree[0][1], nCodes));
  }

  ht.setup(true, false);

  BitPumpMSB bs(data);
  uchar8 *draw = mRaw->getData();
  ushort16 *dest;
  uint32 w = mRaw->dim.x;
  uint32 h = mRaw->dim.y;
  int pUp1[2] = {0, 0};
  int pUp2[2] = {0, 0};
  int pLeft1 = 0;
  int pLeft2 = 0;

  for (uint32 y = 0;y < h;y++) {
    dest = reinterpret_cast<ushort16*>(
        &draw[y * mRaw->pitch]); // Adjust destination
    pUp1[y&1] += ht.decodeNext(bs);
    pUp2[y&1] += ht.decodeNext(bs);
    dest[0] = pLeft1 = pUp1[y&1];
    dest[1] = pLeft2 = pUp2[y&1];
    for (uint32 x = 2; x < w ; x += 2) {
      pLeft1 += ht.decodeNext(bs);
      pLeft2 += ht.decodeNext(bs);
      dest[x] =  pLeft1;
      dest[x+1] =  pLeft2;

      if (pLeft1 < 0 || pLeft1 > 65535)
        ThrowRDE("decoded value out of bounds at %d:%d", x, y);
      if (pLeft2 < 0 || pLeft2 > 65535)
        ThrowRDE("decoded value out of bounds at %d:%d", x, y);
    }
  }
}

} // namespace rawspeed
