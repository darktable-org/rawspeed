/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2014-2015 Pedro Côrte-Real

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

#include "decoders/MrwDecoder.h"
#include "common/Common.h"                // for uchar8, uint32, get2BE
#include "common/Point.h"                 // for iPoint2D
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "decompressors/UncompressedDecompressor.h"
#include "io/ByteStream.h"      // for ByteStream
#include "io/IOException.h"     // for IOException
#include "parsers/TiffParser.h" // for parseTiff
#include "tiff/TiffEntry.h"     // for TiffEntry
#include "tiff/TiffIFD.h"       // for TiffIFD, TiffRootIFD, Tiff...
#include "tiff/TiffTag.h"       // for ::MAKE, ::MODEL
#include <cmath>                // for NAN
#include <cstdio>               // for NULL
#include <map>                  // for map, _Rb_tree_iterator
#include <string>               // for string

using namespace std;

namespace RawSpeed {

class CameraMetaData;

MrwDecoder::MrwDecoder(FileMap* file) :
    RawDecoder(file) {
  tiff_meta = nullptr;
  parseHeader();
}

MrwDecoder::~MrwDecoder() {
  if (tiff_meta)
    delete tiff_meta;
}

int MrwDecoder::isMRW(FileMap* input) {
  const uchar8* data = input->getData(0, 4);
  return data[0] == 0x00 && data[1] == 0x4D && data[2] == 0x52 && data[3] == 0x4D;
}

void MrwDecoder::parseHeader() {
  if (mFile->getSize() < 30)
    ThrowRDE("Not a valid MRW file (size too small)");

  if (!isMRW(mFile))
    ThrowRDE("This isn't actually a MRW file, why are you calling me?");

  const unsigned char* data = mFile->getData(0,8);
  data_offset = get4BE(data,4)+8;
  data = mFile->getData(0,data_offset);

  if (!mFile->isValid(data_offset))
    ThrowRDE("MRW: Data offset is invalid");

  // Make sure all values have at least been initialized
  raw_width = raw_height = packed = 0;
  wb_coeffs[0] = wb_coeffs[1] = wb_coeffs[2] = wb_coeffs[3] = NAN;

  uint32 currpos = 8;
  // At most we read 20 bytes from currpos so check we don't step outside that
  while (currpos+20 < data_offset) {
    uint32 tag = get4BE(data,currpos);
    uint32 len = get4BE(data,currpos+4);
    switch(tag) {
    case 0x505244: // PRD
      raw_height = get2BE(data,currpos+16);
      raw_width = get2BE(data,currpos+18);
      packed = (data[currpos+24] == 12);
    case 0x574247: // WBG
      for(uint32 i=0; i<4; i++)
        wb_coeffs[i] = (float)get2BE(data, currpos+12+i*2);
      break;
    case 0x545457: // TTW
      // Base value for offsets needs to be at the beginning of the TIFF block, not the file
      tiff_meta = parseTiff(mFile->getSubView(currpos+8)).release();
      break;
    }
    currpos += max(len + 8, 1u); // max(,1) to make sure we make progress
  }
}

RawImage MrwDecoder::decodeRawInternal() {
  mRaw->dim = iPoint2D(raw_width, raw_height);
  mRaw->createData();

  UncompressedDecompressor u(*mFile, data_offset, mRaw, uncorrectedRawValues);

  try {
    if (packed)
      u.Decode12BitRawBE(raw_width, raw_height);
    else
      u.Decode12BitRawBEunpacked(raw_width, raw_height);
  } catch (IOException &e) {
    mRaw->setError(e.what());
    // Let's ignore it, it may have delivered somewhat useful data.
  }

  return mRaw;
}

void MrwDecoder::checkSupportInternal(CameraMetaData *meta) {
  if (!tiff_meta || !tiff_meta->hasEntryRecursive(MAKE) || !tiff_meta->hasEntryRecursive(MODEL))
    ThrowRDE("MRW: Couldn't find make and model");

  string make = tiff_meta->getEntryRecursive(MAKE)->getString();
  string model = tiff_meta->getEntryRecursive(MODEL)->getString();
  this->checkCameraSupported(meta, make, model, "");
}

void MrwDecoder::decodeMetaDataInternal(CameraMetaData *meta) {
  //Default
  int iso = 0;

  if (!tiff_meta || !tiff_meta->getEntryRecursive(MAKE) || !tiff_meta->getEntryRecursive(MODEL))
    ThrowRDE("MRW: Couldn't find make and model");

  string make = tiff_meta->getEntryRecursive(MAKE)->getString();
  string model = tiff_meta->getEntryRecursive(MODEL)->getString();

  setMetaData(meta, make, model, "", iso);

  if (hints.find("swapped_wb") != hints.end()) {
    mRaw->metadata.wbCoeffs[0] = (float) wb_coeffs[2];
    mRaw->metadata.wbCoeffs[1] = (float) wb_coeffs[0];
    mRaw->metadata.wbCoeffs[2] = (float) wb_coeffs[1];
  } else {
    mRaw->metadata.wbCoeffs[0] = (float) wb_coeffs[0];
    mRaw->metadata.wbCoeffs[1] = (float) wb_coeffs[1];
    mRaw->metadata.wbCoeffs[2] = (float) wb_coeffs[3];
  }
}

} // namespace RawSpeed
