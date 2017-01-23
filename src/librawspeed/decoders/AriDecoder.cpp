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
*/

#include "decoders/AriDecoder.h"
#include "common/Common.h"                // for uint32, ushort16, getHostE...
#include "common/Point.h"                 // for iPoint2D
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "io/BitPumpMSB32.h"              // for BitPumpMSB32
#include "io/ByteStream.h"                // for ByteStream
#include "io/IOException.h"               // for IOException
#include "metadata/CameraMetaData.h"      // for CameraMetaData
#include "metadata/ColorFilterArray.h"    // for ::CFA_BLUE, ::CFA_GREEN
#include <string>                         // for string

namespace RawSpeed {

AriDecoder::AriDecoder(FileMap* file) : RawDecoder(file) {
  if (mFile->getSize() < 4096) {
    ThrowRDE("ARRI: File too small (no header)");
  }
  try {
    ByteStream s(mFile, 8, getHostEndianness() == little);
    mDataOffset = s.getUInt();
    uint32 someNumber = s.getUInt(); // Value: 3?
    uint32 segmentLength = s.getUInt(); // Value: 0x3c = length
	if (someNumber != 3 || segmentLength != 0x3c) {
		ThrowRDE("Unknown values in ARRIRAW header, %d, %d", someNumber, segmentLength);
	}
    mWidth = s.getUInt();
    mHeight = s.getUInt();
    s.setPosition(0x40);
    mDataSize = s.getUInt();

    // Smells like whitebalance
    s.setPosition(0x5c);
    mWB[0] = s.getFloat();  // 1.3667001 in sample
    mWB[1] = s.getFloat();  // 1.0000000 in sample
    mWB[2] = s.getFloat();  // 1.6450000 in sample

    // Smells like iso
    s.setPosition(0xb8);
    mIso = s.getUInt();  // 100 in sample

    s.setPosition(0x29c-8);
    mModel = s.getString();
    s.setPosition(0x2a4-8);
    mEncoder = s.getString();
  } catch (IOException &e) {
    ThrowRDE("ARRI: IO Exception:%s", e.what());
  }
}

AriDecoder::~AriDecoder() = default;

RawImage AriDecoder::decodeRawInternal() {
  mRaw->dim = iPoint2D(mWidth, mHeight);
  mRaw->createData();

  startThreads();

  mRaw->whitePoint = 4095;
  return mRaw;
}

void AriDecoder::decodeThreaded(RawDecoderThread * t) {
  uint32 startOff = mDataOffset + t->start_y * ((mWidth * 12) / 8);
  BitPumpMSB32 bits(mFile, startOff);

  uint32 hw = mWidth >> 1;
  for (uint32 y = t->start_y; y < t->end_y; y++) {
    auto *dest = (ushort16 *)mRaw->getData(0, y);
    for (uint32 x = 0 ; x < hw; x++) {
      uint32 a = bits.getBits(12);
      uint32 b = bits.getBits(12);
      dest[x*2] = b;
      dest[x*2+1] = a;
      bits.checkPos();
    }
  }
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
  mRaw->metadata.wbCoeffs[0] = mWB[0];
  mRaw->metadata.wbCoeffs[1] = mWB[1];
  mRaw->metadata.wbCoeffs[2] = mWB[2];
  if (meta->hasCamera("ARRI", mModel, mEncoder)) {
    setMetaData(meta, "ARRI", mModel, mEncoder, mIso);
  } else {
    setMetaData(meta, "ARRI", mModel, "", mIso);
  }
}

} // namespace RawSpeed
