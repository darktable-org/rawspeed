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

#include "decompressors/NikonDecompressor.h"
#include "common/Common.h"                // for uint32, ushort16, clampBits
#include "common/Point.h"                 // for iPoint2D
#include "common/RawImage.h"              // for RawImage, RawImageData, RawI...
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "decompressors/HuffmanTable.h"   // for HuffmanTable
#include "io/BitPumpMSB.h"                // for BitPumpMSB, BitStream<>::fil...
#include "io/Buffer.h"                    // for Buffer
#include "io/ByteStream.h"                // for ByteStream
#include <cstdio>                         // for size_t, NULL
#include <vector>                         // for vector, allocator

namespace rawspeed {

const uchar8 NikonDecompressor::nikon_tree[][2][16] = {
    {/* 12-bit lossy */
     {0, 1, 5, 1, 1, 1, 1, 1, 1, 2, 0, 0, 0, 0, 0, 0},
     {5, 4, 3, 6, 2, 7, 1, 0, 8, 9, 11, 10, 12}},
    {/* 12-bit lossy after split */
     {0, 1, 5, 1, 1, 1, 1, 1, 1, 2, 0, 0, 0, 0, 0, 0},
     {0x39, 0x5a, 0x38, 0x27, 0x16, 5, 4, 3, 2, 1, 0, 11, 12, 12}},
    {/* 12-bit lossless */
     {0, 1, 4, 2, 3, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0},
     {5, 4, 6, 3, 7, 2, 8, 1, 9, 0, 10, 11, 12}},
    {/* 14-bit lossy */
     {0, 1, 4, 3, 1, 1, 1, 1, 1, 2, 0, 0, 0, 0, 0, 0},
     {5, 6, 4, 7, 8, 3, 9, 2, 1, 0, 10, 11, 12, 13, 14}},
    {/* 14-bit lossy after split */
     {0, 1, 5, 1, 1, 1, 1, 1, 1, 1, 2, 0, 0, 0, 0, 0},
     {8, 0x5c, 0x4b, 0x3a, 0x29, 7, 6, 5, 4, 3, 2, 1, 0, 13, 14}},
    {/* 14-bit lossless */
     {0, 1, 4, 2, 2, 3, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0},
     {7, 6, 8, 5, 9, 4, 10, 3, 11, 12, 2, 0, 1, 13, 14}},

};

std::vector<ushort16> NikonDecompressor::createCurve(ByteStream* metadata,
                                                     uint32 bitsPS, uint32 v0,
                                                     uint32 v1, uint32* split) {
  // 'curve' will hold a peace wise linearly interpolated function.
  // there are 'csize' segments, each is 'step' values long.
  // the very last value is not part of the used table but necessary
  // to linearly interpolate the last segment, therefore the '+1/-1'
  // size adjustments of 'curve'.
  std::vector<ushort16> curve((1 << bitsPS & 0x7fff) + 1);
  assert(curve.size() > 1);

  for (size_t i = 0; i < curve.size(); i++)
    curve[i] = i;

  uint32 step = 0;
  uint32 csize = metadata->getU16();
  if (csize > 1)
    step = curve.size() / (csize - 1);

  if (v0 == 68 && v1 == 32 && step > 0) {
    if ((csize - 1) * step != curve.size() - 1)
      ThrowRDE("Bad curve segment count (%u)", csize);

    for (size_t i = 0; i < csize; i++)
      curve[i * step] = metadata->getU16();
    for (size_t i = 0; i < curve.size() - 1; i++) {
      const uint32 b_scale = i % step;

      const uint32 a_pos = i - b_scale;
      const uint32 b_pos = a_pos + step;
      assert(a_pos < curve.size());
      assert(b_pos > 0);
      assert(b_pos < curve.size());
      assert(a_pos < b_pos);

      const uint32 a_scale = step - b_scale;
      curve[i] = (a_scale * curve[a_pos] + b_scale * curve[b_pos]) / step;
    }

    metadata->setPosition(562);
    *split = metadata->getU16();
  } else if (v0 != 70) {
    if (csize == 0 || csize > 0x4001)
      ThrowRDE("Don't know how to compute curve! csize = %u", csize);

    curve.resize(csize + 1UL);
    assert(curve.size() > 1);

    for (uint32 i = 0; i < csize; i++) {
      curve[i] = metadata->getU16();
    }
  }

  // and drop the last value
  curve.resize(curve.size() - 1);
  assert(!curve.empty());

  return curve;
}

HuffmanTable NikonDecompressor::createHuffmanTable(uint32 huffSelect) {
  HuffmanTable ht;
  uint32 count = ht.setNCodesPerLength(Buffer(nikon_tree[huffSelect][0], 16));
  ht.setCodeValues(Buffer(nikon_tree[huffSelect][1], count));
  ht.setup(true, false);
  return ht;
}

NikonDecompressor::NikonDecompressor(const RawImage& raw, uint32 bitsPS_)
    : mRaw(raw), bitsPS(bitsPS_) {
  if (mRaw->getCpp() != 1 || mRaw->getDataType() != TYPE_USHORT16 ||
      mRaw->getBpp() != 2)
    ThrowRDE("Unexpected component count / data type");

  if (mRaw->dim.x == 0 || mRaw->dim.y == 0 || mRaw->dim.x % 2 != 0 ||
      mRaw->dim.x > 8288 || mRaw->dim.y > 5520)
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", mRaw->dim.x,
             mRaw->dim.y);

  switch (bitsPS) {
  case 12:
  case 14:
    break;
  default:
    ThrowRDE("Invalid bpp found: %u", bitsPS);
  }
}

void NikonDecompressor::decompress(ByteStream metadata, const ByteStream& data,
                                   bool uncorrectedRawValues) {
  const iPoint2D& size = mRaw->dim;

  uint32 v0 = metadata.getByte();
  uint32 v1 = metadata.getByte();
  uint32 huffSelect = 0;
  uint32 split = 0;
  int pUp1[2];
  int pUp2[2];

  writeLog(DEBUG_PRIO_EXTRA, "Nef version v0:%u, v1:%u", v0, v1);

  if (v0 == 73 || v1 == 88)
    metadata.skipBytes(2110);

  if (v0 == 70)
    huffSelect = 2;
  if (bitsPS == 14)
    huffSelect += 3;

  pUp1[0] = metadata.getU16();
  pUp1[1] = metadata.getU16();
  pUp2[0] = metadata.getU16();
  pUp2[1] = metadata.getU16();

  HuffmanTable ht = createHuffmanTable(huffSelect);

  auto curve = createCurve(&metadata, bitsPS, v0, v1, &split);
  RawImageCurveGuard curveHandler(&mRaw, curve, uncorrectedRawValues);

  BitPumpMSB bits(data);
  uchar8* draw = mRaw->getData();
  uint32 pitch = mRaw->pitch;

  int pLeft1 = 0;
  int pLeft2 = 0;
  uint32 random = bits.peekBits(24);
  //allow gcc to devirtualize the calls below
  auto* rawdata = reinterpret_cast<RawImageDataU16*>(mRaw.get());

  assert(size.x % 2 == 0);
  assert(size.x >= 2);
  for (uint32 y = 0; y < static_cast<unsigned>(size.y); y++) {
    if (split && y == split) {
      ht = createHuffmanTable(huffSelect + 1);
    }
    auto* dest =
        reinterpret_cast<ushort16*>(&draw[y * pitch]); // Adjust destination
    pUp1[y&1] += ht.decodeNext(bits);
    pUp2[y&1] += ht.decodeNext(bits);
    pLeft1 = pUp1[y&1];
    pLeft2 = pUp2[y&1];

    rawdata->setWithLookUp(clampBits(pLeft1, 15),
                           reinterpret_cast<uchar8*>(dest + 0), &random);
    rawdata->setWithLookUp(clampBits(pLeft2, 15),
                           reinterpret_cast<uchar8*>(dest + 1), &random);

    dest += 2;

    for (uint32 x = 2; x < static_cast<uint32>(size.x); x += 2) {
      pLeft1 += ht.decodeNext(bits);
      pLeft2 += ht.decodeNext(bits);

      rawdata->setWithLookUp(clampBits(pLeft1, 15),
                             reinterpret_cast<uchar8*>(dest + 0), &random);
      rawdata->setWithLookUp(clampBits(pLeft2, 15),
                             reinterpret_cast<uchar8*>(dest + 1), &random);

      dest += 2;
    }
  }
}

} // namespace rawspeed
