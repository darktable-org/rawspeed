/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2014 Pedro Côrte-Real

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

#include "decoders/ErfDecoder.h"
#include "common/Common.h"                // for uint32
#include "common/Point.h"                 // for iPoint2D
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "decompressors/UncompressedDecompressor.h"
#include "io/ByteStream.h"  // for ByteStream
#include "tiff/TiffEntry.h" // for TiffEntry
#include "tiff/TiffIFD.h"   // for TiffIFD
#include "tiff/TiffTag.h"   // for ::MODEL, ::MAKE, ::EPSONWB
#include <string>           // for string
#include <vector>           // for vector

using namespace std;

namespace RawSpeed {

class CameraMetaData;

ErfDecoder::ErfDecoder(TiffIFD *rootIFD, FileMap* file)  :
    RawDecoder(file), mRootIFD(rootIFD) {
}

ErfDecoder::~ErfDecoder() { delete mRootIFD; }

RawImage ErfDecoder::decodeRawInternal() {
  vector<TiffIFD*> data = mRootIFD->getIFDsWithTag(STRIPOFFSETS);

  if (data.size() < 2)
    ThrowRDE("ERF Decoder: No image data found");

  TiffIFD* raw = data[1];
  uint32 width = raw->getEntry(IMAGEWIDTH)->getInt();
  uint32 height = raw->getEntry(IMAGELENGTH)->getInt();
  uint32 off = raw->getEntry(STRIPOFFSETS)->getInt();
  uint32 c2 = raw->getEntry(STRIPBYTECOUNTS)->getInt();

  if (c2 > mFile->getSize() - off) {
    mRaw->setError("Warning: byte count larger than file size, file probably truncated.");
  }

  mRaw->dim = iPoint2D(width, height);
  mRaw->createData();

  UncompressedDecompressor u(*mFile, off, c2, mRaw, uncorrectedRawValues);

  u.decode12BitRawBEWithControl(width, height);

  return mRaw;
}

void ErfDecoder::checkSupportInternal(CameraMetaData *meta) {
  vector<TiffIFD*> data = mRootIFD->getIFDsWithTag(MODEL);
  if (data.empty())
    ThrowRDE("ERF Support check: Model name not found");
  string make = data[0]->getEntry(MAKE)->getString();
  string model = data[0]->getEntry(MODEL)->getString();
  this->checkCameraSupported(meta, make, model, "");
}

void ErfDecoder::decodeMetaDataInternal(CameraMetaData *meta) {
  vector<TiffIFD*> data = mRootIFD->getIFDsWithTag(MODEL);

  if (data.empty())
    ThrowRDE("ERF Decoder: Model name found");
  if (!data[0]->hasEntry(MAKE))
    ThrowRDE("ERF Decoder: Make name not found");

  string make = data[0]->getEntry(MAKE)->getString();
  string model = data[0]->getEntry(MODEL)->getString();
  setMetaData(meta, make, model, "", 0);

  if (mRootIFD->hasEntryRecursive(EPSONWB)) {
    TiffEntry *wb = mRootIFD->getEntryRecursive(EPSONWB);
    if (wb->count == 256) {
      // Magic values taken directly from dcraw
      mRaw->metadata.wbCoeffs[0] = (float) wb->getShort(24) * 508.0f * 1.078f / (float)0x10000;
      mRaw->metadata.wbCoeffs[1] = 1.0f;
      mRaw->metadata.wbCoeffs[2] = (float) wb->getShort(25) * 382.0f * 1.173f / (float)0x10000;
    }
  }
}

} // namespace RawSpeed
