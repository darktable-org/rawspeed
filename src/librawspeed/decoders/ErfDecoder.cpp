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
#include "common/Common.h"                          // for uint32
#include "common/Point.h"                           // for iPoint2D
#include "decompressors/UncompressedDecompressor.h" // for UncompressedDeco...
#include "io/Buffer.h"                              // for Buffer
#include "io/Endianness.h"                          // for Endianness
#include "tiff/TiffEntry.h"                         // for TiffEntry
#include "tiff/TiffIFD.h"                           // for TiffRootIFD, Tif...
#include "tiff/TiffTag.h"                           // for TiffTag::EPSONWB
#include <memory>                                   // for unique_ptr

using namespace std;

namespace RawSpeed {

class CameraMetaData;

RawImage ErfDecoder::decodeRawInternal() {
  auto raw = mRootIFD->getIFDWithTag(STRIPOFFSETS, 1);
  uint32 width = raw->getEntry(IMAGEWIDTH)->getU32();
  uint32 height = raw->getEntry(IMAGELENGTH)->getU32();
  uint32 off = raw->getEntry(STRIPOFFSETS)->getU32();
  uint32 c2 = raw->getEntry(STRIPBYTECOUNTS)->getU32();

  if (c2 > mFile->getSize() - off) {
    mRaw->setError("Warning: byte count larger than file size, file probably truncated.");
  }

  mRaw->dim = iPoint2D(width, height);
  mRaw->createData();

  UncompressedDecompressor u(*mFile, off, c2, mRaw);

  u.decode12BitRaw<big, false, true>(width, height);

  return mRaw;
}

void ErfDecoder::decodeMetaDataInternal(const CameraMetaData* meta) {
  setMetaData(meta, "", 0);

  if (mRootIFD->hasEntryRecursive(EPSONWB)) {
    TiffEntry *wb = mRootIFD->getEntryRecursive(EPSONWB);
    if (wb->count == 256) {
      // Magic values taken directly from dcraw
      mRaw->metadata.wbCoeffs[0] = (float) wb->getU16(24) * 508.0f * 1.078f / (float)0x10000;
      mRaw->metadata.wbCoeffs[1] = 1.0f;
      mRaw->metadata.wbCoeffs[2] = (float) wb->getU16(25) * 382.0f * 1.173f / (float)0x10000;
    }
  }
}

} // namespace RawSpeed
