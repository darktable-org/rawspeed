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
#include "common/Common.h"                          // for uchar8, uint32
#include "common/Point.h"                           // for iPoint2D
#include "decoders/RawDecoderException.h"           // for RawDecoderExcept...
#include "decompressors/UncompressedDecompressor.h" // for UncompressedDeco...
#include "io/Buffer.h"                              // for Buffer
#include "io/Endianness.h"                          // for getU16BE, getU32BE
#include "io/IOException.h"                         // for IOException
#include "metadata/Camera.h"                        // for Hints
#include "parsers/TiffParser.h"                     // for parseTiff
#include "tiff/TiffEntry.h"                         // IWYU pragma: keep
#include "tiff/TiffIFD.h"                           // for TiffID, TiffRoot...
#include <algorithm>                                // for max
#include <cmath>                                    // for NAN
#include <memory>                                   // for unique_ptr

using namespace std;

namespace RawSpeed {

class CameraMetaData;

MrwDecoder::MrwDecoder(Buffer* file) : RawDecoder(file) { parseHeader(); }

int MrwDecoder::isMRW(Buffer* input) {
  const uchar8* data = input->getData(0, 4);
  return data[0] == 0x00 && data[1] == 0x4D && data[2] == 0x52 && data[3] == 0x4D;
}

void MrwDecoder::parseHeader() {
  if (mFile->getSize() < 30)
    ThrowRDE("Not a valid MRW file (size too small)");

  if (!isMRW(mFile))
    ThrowRDE("This isn't actually a MRW file, why are you calling me?");

  const unsigned char* data = mFile->getData(0,8);
  data_offset = getU32BE(data + 4) + 8;
  data = mFile->getData(0,data_offset);

  if (!mFile->isValid(data_offset))
    ThrowRDE("Data offset is invalid");

  // Make sure all values have at least been initialized
  raw_width = raw_height = packed = 0;
  wb_coeffs[0] = wb_coeffs[1] = wb_coeffs[2] = wb_coeffs[3] = NAN;

  uint32 currpos = 8;
  // At most we read 20 bytes from currpos so check we don't step outside that
  while (currpos + 20 < data_offset) {
    uint32 tag = getU32BE(data + currpos);
    uint32 len = getU32BE(data + currpos + 4);
    switch(tag) {
    case 0x505244: // PRD
      raw_height = getU16BE(data + currpos + 16);
      raw_width = getU16BE(data + currpos + 18);
      packed = (data[currpos+24] == 12);
      break;
    case 0x574247: // WBG
      for (uint32 i = 0; i < 4; i++)
        wb_coeffs[i] = (float)getU16BE(data + currpos + 12 + i * 2);
      break;
    case 0x545457: // TTW
      // Base value for offsets needs to be at the beginning of the TIFF block, not the file
      rootIFD = parseTiff(mFile->getSubView(currpos+8));
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
      u.decode12BitRawBE(raw_width, raw_height);
    else
      u.decode12BitRawBEunpacked(raw_width, raw_height);
  } catch (IOException &e) {
    mRaw->setError(e.what());
    // Let's ignore it, it may have delivered somewhat useful data.
  }

  return mRaw;
}

void MrwDecoder::checkSupportInternal(const CameraMetaData* meta) {
  auto id = rootIFD->getID();
  this->checkCameraSupported(meta, id.make, id.model, "");
}

void MrwDecoder::decodeMetaDataInternal(const CameraMetaData* meta) {
  //Default
  int iso = 0;

  if (!rootIFD)
    ThrowRDE("Couldn't find make and model");

  auto id = rootIFD->getID();
  setMetaData(meta, id.make, id.model, "", iso);

  if (hints.has("swapped_wb")) {
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
