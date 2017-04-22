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
#include "decompressors/UncompressedDecompressor.h" // for UncompressedDeco...
#include "io/Endianness.h"                          // for Endianness::big
#include "tiff/TiffEntry.h"                         // for TiffEntry
#include "tiff/TiffIFD.h"                           // for TiffRootIFD
#include "tiff/TiffTag.h"                           // for TiffTag::EPSONWB
#include <memory>                                   // for unique_ptr
#include <string>                                   // for operator==, string

namespace rawspeed {

class CameraMetaData;

bool ErfDecoder::isAppropriateDecoder(const TiffRootIFD* rootIFD,
                                      const Buffer* file) {
  const auto id = rootIFD->getID();
  const std::string& make = id.make;

  // FIXME: magic

  return make == "SEIKO EPSON CORP.";
}

RawImage ErfDecoder::decodeRawInternal() {
  SimpleTiffDecoder::prepareForRawDecoding();

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
      mRaw->metadata.wbCoeffs[0] = static_cast<float>(wb->getU16(24)) * 508.0F *
                                   1.078F / static_cast<float>(0x10000);
      mRaw->metadata.wbCoeffs[1] = 1.0F;
      mRaw->metadata.wbCoeffs[2] = static_cast<float>(wb->getU16(25)) * 382.0F *
                                   1.173F / static_cast<float>(0x10000);
    }
  }
}

} // namespace rawspeed
