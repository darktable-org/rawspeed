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

#include "decoders/Rw2Decoder.h"
#include "common/Common.h"                          // for writeLog, uint32
#include "common/Point.h"                           // for iPoint2D
#include "decoders/RawDecoderException.h"           // for ThrowRDE
#include "decompressors/PanasonicDecompressor.h"    // for PanasonicDecompr...
#include "decompressors/PanasonicDecompressorV5.h"    // for PanasonicDecompr...
#include "decompressors/UncompressedDecompressor.h" // for UncompressedDeco...
#include "io/Buffer.h"                              // for Buffer
#include "io/ByteStream.h"                          // for ByteStream
#include "io/Endianness.h"                          // for Endianness, Endi...
#include "metadata/Camera.h"                        // for Hints
#include "metadata/ColorFilterArray.h"              // for CFA_GREEN, Color...
#include "tiff/TiffEntry.h"                         // for TiffEntry
#include "tiff/TiffIFD.h"                           // for TiffRootIFD, Tif...
#include "tiff/TiffTag.h"                           // for TiffTag, PANASON...
#include <array>                                    // for array
#include <cmath>                                    // for fabs
#include <memory>                                   // for unique_ptr
#include <string>                                   // for string, operator==

using std::string;
using std::fabs;

namespace rawspeed {

class CameraMetaData;

bool Rw2Decoder::isAppropriateDecoder(const TiffRootIFD* rootIFD,
                                      const Buffer* file) {
  const auto id = rootIFD->getID();
  const std::string& make = id.make;

  // FIXME: magic

  return make == "Panasonic" || make == "LEICA";
}

RawImage Rw2Decoder::decodeRawInternal() {

  const TiffIFD* raw = nullptr;
  bool isOldPanasonic = ! mRootIFD->hasEntryRecursive(PANASONIC_STRIPOFFSET);

  if (! isOldPanasonic)
    raw = mRootIFD->getIFDWithTag(PANASONIC_STRIPOFFSET);
  else
    raw = mRootIFD->getIFDWithTag(STRIPOFFSETS);

  uint32 height = raw->getEntry(static_cast<TiffTag>(3))->getU16();
  uint32 width = raw->getEntry(static_cast<TiffTag>(2))->getU16();

  if (isOldPanasonic) {
    if (width == 0 || height == 0 || width > 4330 || height > 2751)
      ThrowRDE("Unexpected image dimensions found: (%u; %u)", width, height);

    TiffEntry *offsets = raw->getEntry(STRIPOFFSETS);

    if (offsets->count != 1) {
      ThrowRDE("Multiple Strips found: %u", offsets->count);
    }
    offset = offsets->getU32();
    if (!mFile->isValid(offset))
      ThrowRDE("Invalid image data offset, cannot decode.");

    mRaw->dim = iPoint2D(width, height);

    uint32 size = mFile->getSize() - offset;

    UncompressedDecompressor u(ByteStream(mFile, offset), mRaw);

    if (size >= width*height*2) {
      // It's completely unpacked little-endian
      mRaw->createData();
      u.decodeRawUnpacked<12, Endianness::little>(width, height);
    } else if (size >= width*height*3/2) {
      // It's a packed format
      mRaw->createData();
      u.decode12BitRaw<Endianness::little, false, true>(width, height);
    } else {
      section_split_offset = 0;
      PanasonicDecompressor p(mRaw, ByteStream(mFile, offset),
                              hints.has("zero_is_not_bad"), section_split_offset);
      mRaw->createData();
      p.decompress();
    }
  } else {
    mRaw->dim = iPoint2D(width, height);

    TiffEntry *offsets = raw->getEntry(PANASONIC_STRIPOFFSET);

    if (offsets->count != 1) {
      ThrowRDE("Multiple Strips found: %u", offsets->count);
    }

    offset = offsets->getU32();

    if (!mFile->isValid(offset))
      ThrowRDE("Invalid image data offset, cannot decode.");

    const TiffTag PANASONIC_RAWFORMAT = static_cast<TiffTag>(0x2d);
    bool v5Processing = false;
    if (raw->hasEntry(PANASONIC_RAWFORMAT)) {
      uint32 rawFormat = raw->getEntry(PANASONIC_RAWFORMAT)->getU16();
      if (rawFormat == 5) {
        v5Processing = true;
      }
    }

    const TiffTag PANASONIC_BITSPERSAMPLE = static_cast<TiffTag>(0xa);
    uint32 bitsPerSample = 12;
    if (raw->hasEntry(PANASONIC_BITSPERSAMPLE)) {
      bitsPerSample = raw->getEntry(PANASONIC_BITSPERSAMPLE)->getU16();
    }

    if (v5Processing) {
      PanasonicDecompressorV5 v5(mRaw, ByteStream(mFile, offset),
                              hints.has("zero_is_not_bad"), bitsPerSample);
      mRaw->createData();
      v5.decompress();
    } else {
      section_split_offset = 0x2008;
      PanasonicDecompressor p(mRaw, ByteStream(mFile, offset),
                              hints.has("zero_is_not_bad"), section_split_offset);
      mRaw->createData();
      p.decompress();
    }
  }

  return mRaw;
}

void Rw2Decoder::checkSupportInternal(const CameraMetaData* meta) {
  auto id = mRootIFD->getID();
  if (!checkCameraSupported(meta, id, guessMode()))
    checkCameraSupported(meta, id, "");
}

void Rw2Decoder::decodeMetaDataInternal(const CameraMetaData* meta) {
  mRaw->cfa.setCFA(iPoint2D(2,2), CFA_BLUE, CFA_GREEN, CFA_GREEN, CFA_RED);

  auto id = mRootIFD->getID();
  string mode = guessMode();
  int iso = 0;
  if (mRootIFD->hasEntryRecursive(PANASONIC_ISO_SPEED))
    iso = mRootIFD->getEntryRecursive(PANASONIC_ISO_SPEED)->getU32();

  if (this->checkCameraSupported(meta, id, mode)) {
    setMetaData(meta, id, mode, iso);
  } else {
    mRaw->metadata.mode = mode;
    writeLog(DEBUG_PRIO_EXTRA, "Mode not found in DB: %s", mode.c_str());
    setMetaData(meta, id, "", iso);
  }

  const TiffIFD* raw = mRootIFD->hasEntryRecursive(PANASONIC_STRIPOFFSET)
                           ? mRootIFD->getIFDWithTag(PANASONIC_STRIPOFFSET)
                           : mRootIFD->getIFDWithTag(STRIPOFFSETS);

  // Read blacklevels
  if (raw->hasEntry(static_cast<TiffTag>(0x1c)) &&
      raw->hasEntry(static_cast<TiffTag>(0x1d)) &&
      raw->hasEntry(static_cast<TiffTag>(0x1e))) {
    const auto getBlack = [&raw](TiffTag t) -> int {
      const auto val = raw->getEntry(t)->getU32();
      int out;
      if (__builtin_sadd_overflow(val, 15, &out))
        ThrowRDE("Integer overflow when calculating black level");
      return out;
    };

    const int blackRed = getBlack(static_cast<TiffTag>(0x1c));
    const int blackGreen = getBlack(static_cast<TiffTag>(0x1d));
    const int blackBlue = getBlack(static_cast<TiffTag>(0x1e));

    for(int i = 0; i < 2; i++) {
      for(int j = 0; j < 2; j++) {
        const int k = i + 2 * j;
        const CFAColor c = mRaw->cfa.getColorAt(i, j);
        switch (c) {
          case CFA_RED:
            mRaw->blackLevelSeparate[k] = blackRed;
            break;
          case CFA_GREEN:
            mRaw->blackLevelSeparate[k] = blackGreen;
            break;
          case CFA_BLUE:
            mRaw->blackLevelSeparate[k] = blackBlue;
            break;
          default:
            ThrowRDE("Unexpected CFA color %s.",
                     ColorFilterArray::colorToString(c).c_str());
        }
      }
    }
  }

  // Read WB levels
  if (raw->hasEntry(static_cast<TiffTag>(0x0024)) &&
      raw->hasEntry(static_cast<TiffTag>(0x0025)) &&
      raw->hasEntry(static_cast<TiffTag>(0x0026))) {
    mRaw->metadata.wbCoeffs[0] = static_cast<float>(
        raw->getEntry(static_cast<TiffTag>(0x0024))->getU16());
    mRaw->metadata.wbCoeffs[1] = static_cast<float>(
        raw->getEntry(static_cast<TiffTag>(0x0025))->getU16());
    mRaw->metadata.wbCoeffs[2] = static_cast<float>(
        raw->getEntry(static_cast<TiffTag>(0x0026))->getU16());
  } else if (raw->hasEntry(static_cast<TiffTag>(0x0011)) &&
             raw->hasEntry(static_cast<TiffTag>(0x0012))) {
    mRaw->metadata.wbCoeffs[0] = static_cast<float>(
        raw->getEntry(static_cast<TiffTag>(0x0011))->getU16());
    mRaw->metadata.wbCoeffs[1] = 256.0F;
    mRaw->metadata.wbCoeffs[2] = static_cast<float>(
        raw->getEntry(static_cast<TiffTag>(0x0012))->getU16());
  }
}

std::string Rw2Decoder::guessMode() {
  float ratio = 3.0F / 2.0F; // Default

  if (!mRaw->isAllocated())
    return "";

  ratio = static_cast<float>(mRaw->dim.x) / static_cast<float>(mRaw->dim.y);

  float min_diff = fabs(ratio - 16.0F / 9.0F);
  std::string closest_match = "16:9";

  float t = fabs(ratio - 3.0F / 2.0F);
  if (t < min_diff) {
    closest_match = "3:2";
    min_diff  = t;
  }

  t = fabs(ratio - 4.0F / 3.0F);
  if (t < min_diff) {
    closest_match =  "4:3";
    min_diff  = t;
  }

  t = fabs(ratio - 1.0F);
  if (t < min_diff) {
    closest_match = "1:1";
    min_diff  = t;
  }
  writeLog(DEBUG_PRIO_EXTRA, "Mode guess: '%s'", closest_match.c_str());
  return closest_match;
}

} // namespace rawspeed
