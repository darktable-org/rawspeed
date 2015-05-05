#include "StdAfx.h"
#include "AriDecoder.h"
#include "ByteStreamSwap.h"
/*
RawSpeed - RAW file decoder.

Copyright (C) 2009-2015 Klaus Post

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

  AriDecoder::AriDecoder(FileMap* file) :
RawDecoder(file) {
  // TODO: Check if size is big enough.
  ByteStream *s;
  if (getHostEndianness() == little) {
    s = new ByteStream(mFile->getData(8), mFile->getSize()- 8);
  } else {
    s = new ByteStreamSwap(mFile->getData(8), mFile->getSize()- 8);
  }
  mDataOffset = s->getInt();
  uint32 someNumber = s->getInt(); // Value: 3?
  uint32 segmentLength = s->getInt(); // Value: 0x3c = length
  mWidth = s->getInt();
  mHeight = s->getInt();
  s->setAbsoluteOffset(0x40);
  mDataSize = s->getInt();

  // TODO: Check strings - this is a bit risky.
  s->setAbsoluteOffset(0x29c-8);
  mModel = (const char*)s->getData();
  s->setAbsoluteOffset(0x2a4-8);
  mEncoder = (const char*)s->getData();
}

AriDecoder::~AriDecoder(void) {
}

RawImage AriDecoder::decodeRawInternal() {
  mRaw->dim = iPoint2D(mWidth, mHeight);
  mRaw->createData();

  // TODO: Check if mDataOffset is within file
  // TODO: Check if mDataSize+mDataOffset is within file
  // TODO: Check if mDataSize == mWidth * mHeight * 12 / 8

  ByteStream input(mFile->getData(mDataOffset), mFile->getSize()-mDataOffset);
  BitPumpMSB32 bits(&input);
  uint32 hw = mWidth >> 1;
  for (uint32 y = 0; y < mHeight; y++) {
    ushort16* dest = (ushort16*)mRaw->getData(0, y);
    bits.checkPos();
    for (uint32 x = 0 ; x < hw; x++) {
      uint32 a = bits.getBits(12);
      uint32 b = bits.getBits(12);
      dest[x*2] = b;
      dest[x*2+1] = a;
    }
  }

  return mRaw;
}

void AriDecoder::checkSupportInternal(CameraMetaData *meta) {
  if (meta->hasCamera("ARRI", mModel, mEncoder)) {
    this->checkCameraSupported(meta, "ARRI", mModel, mEncoder);
  } else {
    this->checkCameraSupported(meta, "ARRI", mModel, "");
  }
}

void AriDecoder::decodeMetaDataInternal(CameraMetaData *meta) {
  mRaw->cfa.setCFA(iPoint2D(2,2), CFA_GREEN, CFA_RED, CFA_BLUE, CFA_GREEN2);
  if (meta->hasCamera("ARRI", mModel, mEncoder)) {
    setMetaData(meta, "ARRI", mModel, mEncoder, 0);
  } else {
    setMetaData(meta, "ARRI", mModel, "", 0);
  }
}

} // namespace RawSpeed
