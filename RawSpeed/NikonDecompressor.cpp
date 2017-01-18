#include "StdAfx.h"
#include "NikonDecompressor.h"
#include "BitPumpMSB.h"

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

    http://www.klauspost.com
*/

namespace RawSpeed {

NikonDecompressor::NikonDecompressor(FileMap* file, RawImage img) :
    LJpegDecompressor(file, img) {
}

void NikonDecompressor::initTable(uint32 huffSelect) {
  if (huffmanTableStore.empty())
    huffmanTableStore.emplace_back(make_unique<HuffmanTable>());

  huff[0] = huffmanTableStore.back().get();

  uint32 count = huff[0]->setNCodesPerLength(Buffer(nikon_tree[huffSelect], 16));
  huff[0]->setCodeValues(Buffer(nikon_tree[huffSelect]+16, count));

  huff[0]->setup(mUseBigtable, false);
}

void NikonDecompressor::DecompressNikon(ByteStream *metadata, uint32 w, uint32 h, uint32 bitsPS, uint32 offset, uint32 size) {
  uint32 v0 = metadata->getByte();
  uint32 v1 = metadata->getByte();
  uint32 huffSelect = 0;
  uint32 split = 0;
  int pUp1[2];
  int pUp2[2];
  mUseBigtable = true;

  _RPT2(0, "Nef version v0:%u, v1:%u\n", v0, v1);

  if (v0 == 73 || v1 == 88)
    metadata->skipBytes(2110);

  if (v0 == 70) huffSelect = 2;
  if (bitsPS == 14) huffSelect += 3;

  pUp1[0] = metadata->getShort();
  pUp1[1] = metadata->getShort();
  pUp2[0] = metadata->getShort();
  pUp2[1] = metadata->getShort();

  // 'curve' will hold a peace wise linearly interpolated function.
  // there are 'csize' segements, each is 'step' values long.
  // the very last value is not part of the used table but necessary
  // to linearly interpolate the last segment, therefor the '+1/-1'
  // size adjustments of 'curve'.
  vector<ushort16> curve((1 << bitsPS & 0x7fff)+1);
  for (size_t i = 0; i < curve.size(); i++)
    curve[i] = i;

  uint32 step = 0;
  uint32 csize = metadata->getShort();
  if (csize  > 1)
    step = curve.size() / (csize - 1);
  if (v0 == 68 && v1 == 32 && step > 0) {
    for (uint32 i = 0; i < csize; i++)
      curve[i*step] = metadata->getShort();
    for (size_t i = 0; i < curve.size()-1; i++)
      curve[i] = (curve[i-i%step] * (step - i % step) +
                  curve[i-i%step+step] * (i % step)) / step;
    metadata->setPosition(562);
    split = metadata->getShort();
  } else if (v0 != 70 && csize <= 0x4001) {
    curve.resize(csize+1);
    for (uint32 i = 0; i < csize; i++) {
      curve[i] = metadata->getShort();
    }
  }
  initTable(huffSelect);

  if (!uncorrectedRawValues) {
    mRaw->setTable(&curve[0], curve.size()-1, true);
  }

  uint32 x, y;
  ByteStream input(mFile, offset, size);
  BitPumpMSB bits(input);
  uchar8 *draw = mRaw->getData();
  ushort16 *dest;
  uint32 pitch = mRaw->pitch;

  HuffmanTable *htbl = huff[0];
  int pLeft1 = 0;
  int pLeft2 = 0;
  uint32 cw = w / 2;
  uint32 random = bits.peekBits(24);
  //allow gcc to devirtualize the calls below
  RawImageDataU16* rawdata = (RawImageDataU16*)mRaw.get();
  for (y = 0; y < h; y++) {
    if (split && y == split) {
      initTable(huffSelect + 1);
    }
    dest = (ushort16*) & draw[y*pitch];  // Adjust destination
    pUp1[y&1] += htbl->decodeNext(bits);
    pUp2[y&1] += htbl->decodeNext(bits);
    pLeft1 = pUp1[y&1];
    pLeft2 = pUp2[y&1];
    rawdata->setWithLookUp(clampbits(pLeft1,15), (uchar8*)dest++, &random);
    rawdata->setWithLookUp(clampbits(pLeft2,15), (uchar8*)dest++, &random);
    for (x = 1; x < cw; x++) {
      bits.checkPos();
      pLeft1 += htbl->decodeNext(bits);
      pLeft2 += htbl->decodeNext(bits);
      rawdata->setWithLookUp(clampbits(pLeft1,15), (uchar8*)dest++, &random);
      rawdata->setWithLookUp(clampbits(pLeft2,15), (uchar8*)dest++, &random);
    }
  }

  if (uncorrectedRawValues) {
    mRaw->setTable(&curve[0], curve.size(), false);
  } else {
    mRaw->setTable(NULL);
  }
}

} // namespace RawSpeed
