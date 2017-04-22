/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2015 Pedro Côrte-Real

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

#include "decoders/DcsDecoder.h"
#include "decoders/RawDecoderException.h"           // for RawDecoderExcept...
#include "decompressors/UncompressedDecompressor.h" // for UncompressedDeco...
#include "tiff/TiffEntry.h"                         // for TiffEntry, TiffD...
#include "tiff/TiffIFD.h"                           // for TiffRootIFD
#include "tiff/TiffTag.h"                           // for TiffTag::GRAYRES...
#include <cassert>                                  // for assert
#include <memory>                                   // for unique_ptr
#include <string>                                   // for operator==, string
#include <vector>                                   // for vector

namespace rawspeed {

class CameraMetaData;

bool DcsDecoder::isAppropriateDecoder(const TiffRootIFD* rootIFD,
                                      const Buffer* file) {
  const auto id = rootIFD->getID();
  const std::string& make = id.make;

  // FIXME: magic

  return make == "KODAK";
}

RawImage DcsDecoder::decodeRawInternal() {
  SimpleTiffDecoder::prepareForRawDecoding();

  TiffEntry *linearization = mRootIFD->getEntryRecursive(GRAYRESPONSECURVE);
  if (!linearization || linearization->count != 256 || linearization->type != TIFF_SHORT)
    ThrowRDE("Couldn't find the linearization table");

  assert(linearization != nullptr);
  auto table = linearization->getU16Array(256);

  if (!uncorrectedRawValues)
    mRaw->setTable(table.data(), table.size(), true);

  UncompressedDecompressor u(*mFile, off, c2, mRaw);

  if (uncorrectedRawValues)
    u.decode8BitRaw<true>(width, height);
  else
    u.decode8BitRaw<false>(width, height);

  // Set the table, if it should be needed later.
  if (uncorrectedRawValues) {
    mRaw->setTable(table.data(), table.size(), false);
  } else {
    mRaw->setTable(nullptr);
  }

  return mRaw;
}

void DcsDecoder::decodeMetaDataInternal(const CameraMetaData* meta) {
  setMetaData(meta, "", 0);
}

} // namespace rawspeed
