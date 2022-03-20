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

#include "decoders/ThreefrDecoder.h"
#include "common/Point.h"                         // for iPoint2D
#include "decoders/RawDecoderException.h"         // for ThrowRDE
#include "decompressors/HasselbladDecompressor.h" // for HasselbladDecompre...
#include "io/Buffer.h"                            // for Buffer, DataBuffer
#include "io/ByteStream.h"                        // for ByteStream
#include "io/Endianness.h"                        // for Endianness, Endian...
#include "metadata/Camera.h"                      // for Hints
#include "metadata/ColorFilterArray.h" // for CFAColor::GREEN, CFAColor::BLUE
#include "tiff/TiffEntry.h"                       // for TiffEntry
#include "tiff/TiffIFD.h"                         // for TiffRootIFD, TiffIFD
#include "tiff/TiffTag.h"                         // for ASSHOTNEUTRAL, STR...
#include <array>                                  // for array
#include <cstdint>                                // for uint32_t
#include <memory>                                 // for unique_ptr
#include <string>                                 // for operator==, string

namespace rawspeed {

class CameraMetaData;

bool ThreefrDecoder::isAppropriateDecoder(const TiffRootIFD* rootIFD,
                                          [[maybe_unused]] const Buffer& file) {
  const auto id = rootIFD->getID();
  const std::string& make = id.make;

  // FIXME: magic

  return make == "Hasselblad";
}

void ThreefrDecoder::decodeRawInternal() {
  const auto* raw = mRootIFD->getIFDWithTag(TiffTag::STRIPOFFSETS, 1);
  uint32_t width = raw->getEntry(TiffTag::IMAGEWIDTH)->getU32();
  uint32_t height = raw->getEntry(TiffTag::IMAGELENGTH)->getU32();
  uint32_t off = raw->getEntry(TiffTag::STRIPOFFSETS)->getU32();
  // STRIPBYTECOUNTS is strange/invalid for the existing 3FR samples...

  const ByteStream bs(DataBuffer(mFile.getSubView(off), Endianness::little));

  mRaw->dim = iPoint2D(width, height);

  HasselbladDecompressor l(bs, mRaw.get());
  mRaw->createData();

  int pixelBaseOffset = hints.get("pixelBaseOffset", 0);
  l.decode(pixelBaseOffset);
}

void ThreefrDecoder::decodeMetaDataInternal(const CameraMetaData* meta) {
  mRaw->cfa.setCFA(iPoint2D(2, 2), CFAColor::RED, CFAColor::GREEN,
                   CFAColor::GREEN, CFAColor::BLUE);

  setMetaData(meta, "", 0);

  if (mRootIFD->hasEntryRecursive(TiffTag::BLACKLEVEL)) {
    const TiffEntry* bl = mRootIFD->getEntryRecursive(TiffTag::BLACKLEVEL);
    if (bl->count == 1)
      mRaw->blackLevel = bl->getFloat();
  }

  if (mRootIFD->hasEntryRecursive(TiffTag::WHITELEVEL)) {
    const TiffEntry* wl = mRootIFD->getEntryRecursive(TiffTag::WHITELEVEL);
    if (wl->count == 1)
      mRaw->whitePoint = wl->getFloat();
  }

  // Fetch the white balance
  if (mRootIFD->hasEntryRecursive(TiffTag::ASSHOTNEUTRAL)) {
    const TiffEntry* wb = mRootIFD->getEntryRecursive(TiffTag::ASSHOTNEUTRAL);
    if (wb->count == 3) {
      for (uint32_t i = 0; i < 3; i++) {
        const float div = wb->getFloat(i);
        if (div == 0.0F)
          ThrowRDE("Can not decode WB, multiplier is zero/");

        mRaw->metadata.wbCoeffs[i] = 1.0F / div;
      }
    }
  }
}

} // namespace rawspeed
