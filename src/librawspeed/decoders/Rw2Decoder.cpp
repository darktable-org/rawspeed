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
#include "common/Common.h"                          // for writeLog, DEBUG_...
#include "common/Point.h"                           // for iPoint2D
#include "decoders/RawDecoderException.h"           // for ThrowRDE
#include "decompressors/PanasonicDecompressorV4.h"  // for PanasonicDecompr...
#include "decompressors/PanasonicDecompressorV5.h"  // for PanasonicDecompr...
#include "decompressors/PanasonicDecompressorV6.h"  // for PanasonicDecompr...
#include "decompressors/UncompressedDecompressor.h" // for UncompressedDeco...
#include "io/Buffer.h"                              // for Buffer, DataBuffer
#include "io/ByteStream.h"                          // for ByteStream
#include "io/Endianness.h"                          // for Endianness, Endi...
#include "metadata/Camera.h"                        // for Hints
#include "metadata/ColorFilterArray.h" // for CFAColor::GREEN, Color...
#include "tiff/TiffEntry.h"                         // for TiffEntry
#include "tiff/TiffIFD.h"                           // for TiffIFD, TiffRoo...
#include "tiff/TiffTag.h"                           // for TiffTag, PANASON...
#include <array>                                    // for array
#include <cmath>                                    // IWYU pragma: keep
#include <cstdint>                                  // for uint32_t, uint16_t
#include <memory>                                   // for unique_ptr
#include <string>                                   // for string, operator==

using std::fabs;

namespace rawspeed {

class CameraMetaData;

bool Rw2Decoder::isAppropriateDecoder(const TiffRootIFD* rootIFD,
                                      const Buffer& file) {
  const auto id = rootIFD->getID();
  const std::string& make = id.make;

  // FIXME: magic

  return make == "Panasonic" || make == "LEICA" || make == "LEICA CAMERA AG";
}

RawImage Rw2Decoder::decodeRawInternal() {

  const TiffIFD* raw = nullptr;
  bool isOldPanasonic =
      !mRootIFD->hasEntryRecursive(TiffTag::PANASONIC_STRIPOFFSET);

  if (! isOldPanasonic)
    raw = mRootIFD->getIFDWithTag(TiffTag::PANASONIC_STRIPOFFSET);
  else
    raw = mRootIFD->getIFDWithTag(TiffTag::STRIPOFFSETS);

  uint32_t height = raw->getEntry(static_cast<TiffTag>(3))->getU16();
  uint32_t width = raw->getEntry(static_cast<TiffTag>(2))->getU16();

  if (isOldPanasonic) {
    if (width == 0 || height == 0 || width > 4330 || height > 2751)
      ThrowRDE("Unexpected image dimensions found: (%u; %u)", width, height);

    const TiffEntry* offsets = raw->getEntry(TiffTag::STRIPOFFSETS);

    if (offsets->count != 1) {
      ThrowRDE("Multiple Strips found: %u", offsets->count);
    }
    uint32_t offset = offsets->getU32();
    if (!mFile.isValid(offset))
      ThrowRDE("Invalid image data offset, cannot decode.");

    mRaw->dim = iPoint2D(width, height);

    uint32_t size = mFile.getSize() - offset;

    UncompressedDecompressor u(
        ByteStream(DataBuffer(mFile.getSubView(offset), Endianness::little)),
        mRaw);

    if (size >= width*height*2) {
      // It's completely unpacked little-endian
      mRaw->createData();
      u.decodeRawUnpacked<12, Endianness::little>(width, height);
    } else if (size >= width*height*3/2) {
      // It's a packed format
      mRaw->createData();
      u.decode12BitRaw<Endianness::little, false, true>(width, height);
    } else {
      uint32_t section_split_offset = 0;
      PanasonicDecompressorV4 p(
          mRaw,
          ByteStream(DataBuffer(mFile.getSubView(offset), Endianness::little)),
          hints.has("zero_is_not_bad"), section_split_offset);
      mRaw->createData();
      p.decompress();
    }
  } else {
    mRaw->dim = iPoint2D(width, height);

    const TiffEntry* offsets = raw->getEntry(TiffTag::PANASONIC_STRIPOFFSET);

    if (offsets->count != 1) {
      ThrowRDE("Multiple Strips found: %u", offsets->count);
    }

    uint32_t offset = offsets->getU32();

    ByteStream bs(DataBuffer(mFile.getSubView(offset), Endianness::little));

    uint16_t bitsPerSample = 12;
    if (raw->hasEntry(TiffTag::PANASONIC_BITSPERSAMPLE))
      bitsPerSample = raw->getEntry(TiffTag::PANASONIC_BITSPERSAMPLE)->getU16();

    switch (uint16_t version =
                raw->getEntry(TiffTag::PANASONIC_RAWFORMAT)->getU16()) {
    case 4: {
      uint32_t section_split_offset = 0x1FF8;
      PanasonicDecompressorV4 p(mRaw, bs, hints.has("zero_is_not_bad"),
                                section_split_offset);
      mRaw->createData();
      p.decompress();
      return mRaw;
    }
    case 5: {
      PanasonicDecompressorV5 v5(mRaw, bs, bitsPerSample);
      mRaw->createData();
      v5.decompress();
      return mRaw;
    }
    case 6: {
      PanasonicDecompressorV6 v6(mRaw, bs);
      mRaw->createData();
      v6.decompress();
      return mRaw;
    }
    default:
      ThrowRDE("Version %i is unsupported", version);
    }
  }

  return mRaw;
}

void Rw2Decoder::checkSupportInternal(const CameraMetaData* meta) {
  auto id = mRootIFD->getID();
  if (!checkCameraSupported(meta, id, guessMode()))
    checkCameraSupported(meta, id, "");
}

void Rw2Decoder::parseCFA() const {
  if (!mRootIFD->hasEntryRecursive(TiffTag::PANASONIC_CFAPATTERN))
    ThrowRDE("No PANASONIC_CFAPATTERN entry found!");

  const TiffEntry* CFA =
      mRootIFD->getEntryRecursive(TiffTag::PANASONIC_CFAPATTERN);
  if (CFA->type != TiffDataType::SHORT || CFA->count != 1) {
    ThrowRDE("Bad PANASONIC_CFAPATTERN entry (type %u, count %u).",
             static_cast<unsigned>(CFA->type), CFA->count);
  }

  switch (auto i = CFA->getU16()) {
  case 1:
    mRaw->cfa.setCFA(iPoint2D(2, 2), CFAColor::RED, CFAColor::GREEN,
                     CFAColor::GREEN, CFAColor::BLUE);
    break;
  case 2:
    mRaw->cfa.setCFA(iPoint2D(2, 2), CFAColor::GREEN, CFAColor::RED,
                     CFAColor::BLUE, CFAColor::GREEN);
    break;
  case 3:
    mRaw->cfa.setCFA(iPoint2D(2, 2), CFAColor::GREEN, CFAColor::BLUE,
                     CFAColor::RED, CFAColor::GREEN);
    break;
  case 4:
    mRaw->cfa.setCFA(iPoint2D(2, 2), CFAColor::BLUE, CFAColor::GREEN,
                     CFAColor::GREEN, CFAColor::RED);
    break;
  default:
    ThrowRDE("Unexpected CFA pattern: %u", i);
  }
}

void Rw2Decoder::decodeMetaDataInternal(const CameraMetaData* meta) {
  parseCFA();

  auto id = mRootIFD->getID();
  std::string mode = guessMode();
  int iso = 0;
  if (mRootIFD->hasEntryRecursive(TiffTag::PANASONIC_ISO_SPEED))
    iso = mRootIFD->getEntryRecursive(TiffTag::PANASONIC_ISO_SPEED)->getU32();

  if (this->checkCameraSupported(meta, id, mode)) {
    setMetaData(meta, id, mode, iso);
  } else {
    mRaw->metadata.mode = mode;
    writeLog(DEBUG_PRIO::EXTRA, "Mode not found in DB: %s", mode.c_str());
    setMetaData(meta, id, "", iso);
  }

  const TiffIFD* raw =
      mRootIFD->hasEntryRecursive(TiffTag::PANASONIC_STRIPOFFSET)
          ? mRootIFD->getIFDWithTag(TiffTag::PANASONIC_STRIPOFFSET)
          : mRootIFD->getIFDWithTag(TiffTag::STRIPOFFSETS);

  const uint16_t version = 
      raw->getEntry(TiffTag::PANASONIC_RAWFORMAT)->getU16();

  // Read blacklevels
  if (raw->hasEntry(static_cast<TiffTag>(0x1c)) &&
      raw->hasEntry(static_cast<TiffTag>(0x1d)) &&
      raw->hasEntry(static_cast<TiffTag>(0x1e))) {
    const auto getBlack = [&raw](TiffTag t) {
      const uint32_t val = raw->getEntry(t)->getU16();
      // After version 4 the black levels appears to be correct.
      // Continue adding 15 for older raw versions.
      if(version > 4) {
        return val;
      } else {
        int out;
        if (__builtin_sadd_overflow(val, 15, &out))
          ThrowRDE("Integer overflow when calculating black level");
        return out;
      }
    };

    const int blackRed = getBlack(static_cast<TiffTag>(0x1c));
    const int blackGreen = getBlack(static_cast<TiffTag>(0x1d));
    const int blackBlue = getBlack(static_cast<TiffTag>(0x1e));

    for(int i = 0; i < 2; i++) {
      for(int j = 0; j < 2; j++) {
        const int k = i + 2 * j;
        const CFAColor c = mRaw->cfa.getColorAt(i, j);
        switch (c) {
        case CFAColor::RED:
          mRaw->blackLevelSeparate[k] = blackRed;
          break;
        case CFAColor::GREEN:
          mRaw->blackLevelSeparate[k] = blackGreen;
          break;
        case CFAColor::BLUE:
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

std::string Rw2Decoder::guessMode() const {
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
  writeLog(DEBUG_PRIO::EXTRA, "Mode guess: '%s'", closest_match.c_str());
  return closest_match;
}

} // namespace rawspeed
