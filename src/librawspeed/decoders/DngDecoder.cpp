/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post

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

#include "rawspeedconfig.h" // for HAVE_JPEG, HAVE_ZLIB
#include "decoders/DngDecoder.h"
#include "common/Common.h"                // for uint32, uchar8, writeLog
#include "common/DngOpcodes.h"            // for DngOpcodes
#include "common/Point.h"                 // for iPoint2D, iRectangle2D
#include "common/RawspeedException.h"     // for RawspeedException
#include "decoders/DngDecoderSlices.h"    // for DngDecoderSlices, DngSlice...
#include "decoders/RawDecoderException.h" // for ThrowRDE, RawDecoderException
#include "io/Buffer.h"                    // for Buffer
#include "metadata/BlackArea.h"           // for BlackArea
#include "metadata/Camera.h"              // for Camera
#include "metadata/CameraMetaData.h"      // for CameraMetaData
#include "metadata/ColorFilterArray.h"    // for CFAColor, ColorFilterArray
#include "tiff/TiffEntry.h"               // for TiffEntry, TiffDataType::T...
#include "tiff/TiffIFD.h"                 // for TiffIFD, TiffRootIFD, TiffID
#include "tiff/TiffTag.h"                 // for TiffTag::UNIQUECAMERAMODEL
#include <algorithm>                      // for move
#include <cassert>                        // for assert
#include <cstdio>                         // for printf
#include <cstring>                        // for memset
#include <map>                            // for map
#include <memory>                         // for unique_ptr, allocator_trai...
#include <stdexcept>                      // for out_of_range
#include <string>                         // for string, operator+, basic_s...
#include <vector>                         // for vector, allocator

using std::vector;
using std::map;
using std::string;

namespace rawspeed {

DngDecoder::DngDecoder(TiffRootIFDOwner&& rootIFD, Buffer* file)
    : AbstractTiffDecoder(move(rootIFD), file) {
  const uchar8* v = mRootIFD->getEntryRecursive(DNGVERSION)->getData(4);

  if (v[0] != 1)
    ThrowRDE("Not a supported DNG image format: v%u.%u.%u.%u", (int)v[0], (int)v[1], (int)v[2], (int)v[3]);
//  if (v[1] > 4)
//    ThrowRDE("Not a supported DNG image format: v%u.%u.%u.%u", (int)v[0], (int)v[1], (int)v[2], (int)v[3]);

  if ((v[0] <= 1) && (v[1] < 1))  // Prior to v1.1.xxx  fix LJPEG encoding bug
    mFixLjpeg = true;
  else
    mFixLjpeg = false;
}

void DngDecoder::dropUnsuportedChunks(std::vector<const TiffIFD*>* data) {
  for (auto i = data->begin(); i != data->end();) {
    const auto& ifd = *i;

    int comp = ifd->getEntry(COMPRESSION)->getU16();
    bool isSubsampled = false;
    bool isAlpha = false;

    if (ifd->hasEntry(NEWSUBFILETYPE) &&
        ifd->getEntry(NEWSUBFILETYPE)->isInt()) {
      const uint32 NewSubFileType = (*i)->getEntry(NEWSUBFILETYPE)->getU32();

      // bit 0 is on if image is subsampled.
      // the value itself can be either 1, or 0x10001.
      // or 5 for "Transparency information for subsampled raw images"
      isSubsampled = NewSubFileType & (1 << 0);

      // bit 2 is on if image contains transparency information.
      // the value itself can be either 4 or 5
      isAlpha = NewSubFileType & (1 << 2);

      assert((NewSubFileType == 0) || isSubsampled || isAlpha);
    }

    // normal raw?
    bool supported = !isSubsampled && !isAlpha;

    switch (comp) {
    case 1: // uncompressed
    case 7: // lossless JPEG
#ifdef HAVE_ZLIB
    case 8: // deflate
#endif
#ifdef HAVE_JPEG
    case 0x884c: // lossy JPEG
#endif
      // no change, if supported, then is still supported.
      break;

#ifndef HAVE_ZLIB
    case 8: // deflate
#pragma message                                                                \
    "ZLIB is not present! Deflate compression will not be supported!"
      writeLog(DEBUG_PRIO_WARNING, "DNG Decoder: found Deflate-encoded chunk, "
                                   "but the deflate support was disabled at "
                                   "build!");
#endif
#ifndef HAVE_JPEG
    case 0x884c: // lossy JPEG
#pragma message                                                                \
    "JPEG is not present! Lossy JPEG compression will not be supported!"
      writeLog(DEBUG_PRIO_WARNING, "DNG Decoder: found lossy JPEG-encoded "
                                   "chunk, but the jpeg support was "
                                   "disabled at build!");
#endif
    default:
      supported = false;
      break;
    }

    if (supported)
      ++i;
    else
      i = data->erase(i);
  }
}

void DngDecoder::parseCFA(const TiffIFD* raw) {

  // Check if layout is OK, if present
  if (raw->hasEntry(CFALAYOUT) && raw->getEntry(CFALAYOUT)->getU16() != 1)
    ThrowRDE("Unsupported CFA Layout.");

  TiffEntry* cfadim = raw->getEntry(CFAREPEATPATTERNDIM);
  if (cfadim->count != 2)
    ThrowRDE("Couldn't read CFA pattern dimension");

  // Does NOT contain dimensions as some documents state
  TiffEntry* cPat = raw->getEntry(CFAPATTERN);

  iPoint2D cfaSize(cfadim->getU32(1), cfadim->getU32(0));
  if (cfaSize.area() != cPat->count) {
    ThrowRDE("CFA pattern dimension and pattern count does not "
             "match: %d.",
             cPat->count);
  }

  mRaw->cfa.setSize(cfaSize);

  static const map<uint32, CFAColor> int2enum = {
      {0, CFA_RED},     {1, CFA_GREEN},  {2, CFA_BLUE},  {3, CFA_CYAN},
      {4, CFA_MAGENTA}, {5, CFA_YELLOW}, {6, CFA_WHITE},
  };

  for (int y = 0; y < cfaSize.y; y++) {
    for (int x = 0; x < cfaSize.x; x++) {
      uint32 c1 = cPat->getByte(x + y * cfaSize.x);
      CFAColor c2 = CFA_UNKNOWN;

      try {
        c2 = int2enum.at(c1);
      } catch (std::out_of_range&) {
        ThrowRDE("Unsupported CFA Color: %u", c1);
      }

      mRaw->cfa.setColorAt(iPoint2D(x, y), c2);
    }
  }
}

void DngDecoder::decodeData(const TiffIFD* raw, int compression, uint32 sample_format) {
  if (compression == 8 && sample_format != 3) {
    ThrowRDE("Only float format is supported for "
             "deflate-compressed data.");
  } else if ((compression == 7 || compression == 0x884c) &&
             sample_format != 1) {
    ThrowRDE("Only 16 bit unsigned data supported for "
             "JPEG-compressed data.");
  }

  DngDecoderSlices slices(mFile, mRaw, compression);
  if (raw->hasEntry(PREDICTOR)) {
    uint32 predictor = raw->getEntry(PREDICTOR)->getU32();
    slices.mPredictor = predictor;
  }
  slices.mBps = bps;
  if (raw->hasEntry(TILEOFFSETS)) {
    uint32 tilew = raw->getEntry(TILEWIDTH)->getU32();
    uint32 tileh = raw->getEntry(TILELENGTH)->getU32();

    if (!(tilew > 0 && tileh > 0))
      ThrowRDE("Invalid tile size: (%u, %u)", tilew, tileh);

    assert(tilew > 0);
    uint32 tilesX = (mRaw->dim.x + tilew - 1) / tilew;
    assert(tilesX > 0);

    assert(tileh > 0);
    uint32 tilesY = (mRaw->dim.y + tileh - 1) / tileh;
    assert(tilesY > 0);

    uint32 nTiles = tilesX * tilesY;
    assert(nTiles > 0);

    TiffEntry* offsets = raw->getEntry(TILEOFFSETS);
    TiffEntry* counts = raw->getEntry(TILEBYTECOUNTS);
    if (offsets->count != counts->count || offsets->count != nTiles) {
      ThrowRDE("Tile count mismatch: offsets:%u count:%u, "
               "calculated:%u",
               offsets->count, counts->count, nTiles);
    }

    slices.mFixLjpeg = mFixLjpeg;

    for (uint32 y = 0; y < tilesY; y++) {
      for (uint32 x = 0; x < tilesX; x++) {
        const auto s = x + y * tilesX;
        const auto offset = offsets->getU32(s);
        const auto count = counts->getU32(s);

        if (count < 1)
          continue;

        const uint32 offX = tilew * x;
        const uint32 offY = tileh * y;

        auto e = make_unique<DngSliceElement>(offset, count, offX, offY, tilew,
                                              tileh);

        // Only decode if size is valid
        if (mFile->isValid(e->byteOffset, e->byteCount))
          slices.addSlice(move(e));
      }
    }
  } else { // Strips
    TiffEntry* offsets = raw->getEntry(STRIPOFFSETS);
    TiffEntry* counts = raw->getEntry(STRIPBYTECOUNTS);

    if (counts->count != offsets->count) {
      ThrowRDE("Byte count number does not match strip size: "
               "count:%u, stips:%u ",
               counts->count, offsets->count);
    }

    uint32 yPerSlice = raw->hasEntry(ROWSPERSTRIP) ?
          raw->getEntry(ROWSPERSTRIP)->getU32() : mRaw->dim.y;

    if (yPerSlice == 0 || yPerSlice > static_cast<uint32>(mRaw->dim.y))
      ThrowRDE("Invalid y per slice");

    uint32 offY = 0;
    for (uint32 s = 0; s < counts->count; s++) {
      const auto offset = offsets->getU32(s);
      const auto count = counts->getU32(s);

      if (count < 1)
        continue;

      auto e = make_unique<DngSliceElement>(offset, count, 0, offY, mRaw->dim.x,
                                            yPerSlice);

      offY += yPerSlice;

      // Only decode if size is valid
      if (mFile->isValid(e->byteOffset, e->byteCount))
        slices.addSlice(move(e));
    }
  }
  uint32 nSlices = slices.size();
  if (!nSlices)
    ThrowRDE("No valid slices found.");

  mRaw->createData();

  slices.startDecoding();

  if (mRaw->errors.size() >= nSlices) {
    ThrowRDE("Too many errors encountered. Giving up. First Error:\n%s",
             mRaw->errors[0].c_str());
  }
}

RawImage DngDecoder::decodeRawInternal() {
  vector<const TiffIFD*> data = mRootIFD->getIFDsWithTag(COMPRESSION);

  if (data.empty())
    ThrowRDE("No image data found");

  dropUnsuportedChunks(&data);

  if (data.empty())
    ThrowRDE("No RAW chunks found");

  if (data.size() > 1) {
    writeLog(DEBUG_PRIO_EXTRA, "Multiple RAW chunks found - using first only!");
  }

  const TiffIFD* raw = data[0];

  bps = raw->getEntry(BITSPERSAMPLE)->getU32();
  if (bps < 1 || bps > 32)
    ThrowRDE("Unsupported bit per sample count: %u.", bps);

  uint32 sample_format = 1;
  if (raw->hasEntry(SAMPLEFORMAT))
    sample_format = raw->getEntry(SAMPLEFORMAT)->getU32();

  int compression = raw->getEntry(COMPRESSION)->getU16();

  switch (sample_format) {
  case 1:
    mRaw = RawImage::create(TYPE_USHORT16);
    break;
  case 3:
    mRaw = RawImage::create(TYPE_FLOAT32);
    break;
  default:
    ThrowRDE("Only 16 bit unsigned or float point data supported. Sample "
             "format %u is not supported.",
             sample_format);
  }

  mRaw->isCFA = (raw->getEntry(PHOTOMETRICINTERPRETATION)->getU16() == 32803);

  if (mRaw->isCFA)
    writeLog(DEBUG_PRIO_EXTRA, "This is a CFA image");
  else {
    writeLog(DEBUG_PRIO_EXTRA, "This is NOT a CFA image");
  }

  if (sample_format == 1 && bps > 16)
    ThrowRDE("Integer precision larger than 16 bits currently not supported.");

  if (sample_format == 3 && bps != 32 && compression != 8)
    ThrowRDE("Uncompressed float point must be 32 bits per sample.");

  mRaw->dim.x = raw->getEntry(IMAGEWIDTH)->getU32();
  mRaw->dim.y = raw->getEntry(IMAGELENGTH)->getU32();

  if (mRaw->isCFA)
    parseCFA(raw);

  uint32 cpp = raw->getEntry(SAMPLESPERPIXEL)->getU32();

  if (cpp < 1 || cpp > 4)
    ThrowRDE("Unsupported samples per pixel count: %u.", cpp);

  mRaw->setCpp(cpp);

  // Now load the image
  decodeData(raw, compression, sample_format);

  // Crop
  if (raw->hasEntry(ACTIVEAREA)) {
    iPoint2D new_size(mRaw->dim.x, mRaw->dim.y);

    TiffEntry *active_area = raw->getEntry(ACTIVEAREA);
    if (active_area->count != 4)
      ThrowRDE("active area has %d values instead of 4", active_area->count);

    auto corners = active_area->getU32Array(4);
    if (iPoint2D(corners[1], corners[0]).isThisInside(mRaw->dim) &&
        iPoint2D(corners[3], corners[2]).isThisInside(mRaw->dim)) {
      iRectangle2D crop(corners[1], corners[0], corners[3] - corners[1],
                        corners[2] - corners[0]);
      mRaw->subFrame(crop);
    }
  }

  if (raw->hasEntry(DEFAULTCROPORIGIN) && raw->hasEntry(DEFAULTCROPSIZE)) {
    iRectangle2D cropped(0, 0, mRaw->dim.x, mRaw->dim.y);
    TiffEntry *origin_entry = raw->getEntry(DEFAULTCROPORIGIN);
    TiffEntry *size_entry = raw->getEntry(DEFAULTCROPSIZE);

    /* Read crop position (sometimes is rational so use float) */
    auto tl = origin_entry->getFloatArray(2);
    if (iPoint2D(tl[0], tl[1]).isThisInside(mRaw->dim))
      cropped = iRectangle2D(tl[0], tl[1], 0, 0);

    cropped.dim = mRaw->dim - cropped.pos;
    /* Read size (sometimes is rational so use float) */
    auto sz = size_entry->getFloatArray(2);
    iPoint2D size(sz[0], sz[1]);
    if ((size + cropped.pos).isThisInside(mRaw->dim))
      cropped.dim = size;

    if (!cropped.hasPositiveArea())
      ThrowRDE("No positive crop area");

    mRaw->subFrame(cropped);
  }
  if (mRaw->dim.area() <= 0)
    ThrowRDE("No image left after crop");

  // Apply stage 1 opcodes
  if (applyStage1DngOpcodes && raw->hasEntry(OPCODELIST1)) {
    try {
      DngOpcodes codes(raw->getEntry(OPCODELIST1));
      codes.applyOpCodes(mRaw);
    } catch (RawDecoderException& e) {
      // We push back errors from the opcode parser, since the image may still
      // be usable
      mRaw->setError(e.what());
    }
  }

  // Linearization
  if (raw->hasEntry(LINEARIZATIONTABLE) &&
      raw->getEntry(LINEARIZATIONTABLE)->count > 0) {
    TiffEntry *lintable = raw->getEntry(LINEARIZATIONTABLE);
    auto table = lintable->getU16Array(lintable->count);
    mRaw->setTable(table.data(), table.size(), !uncorrectedRawValues);
    if (!uncorrectedRawValues) {
      mRaw->sixteenBitLookup();
      mRaw->setTable(nullptr);
    }

    if (false) { // NOLINT else would need preprocessor
      // Test average for bias
      uint32 cw = mRaw->dim.x * mRaw->getCpp();
      auto* pixels = reinterpret_cast<ushort16*>(mRaw->getData(0, 500));
      float avg = 0.0F;
      for (uint32 x = 0; x < cw; x++) {
        avg += static_cast<float>(pixels[x]);
      }
      printf("Average:%f\n", avg / static_cast<float>(cw));
    }
  }

 // Default white level is (2 ** BitsPerSample) - 1
  mRaw->whitePoint = (1UL << bps) - 1UL;

  if (raw->hasEntry(WHITELEVEL)) {
    TiffEntry *whitelevel = raw->getEntry(WHITELEVEL);
    if (whitelevel->isInt())
      mRaw->whitePoint = whitelevel->getU32();
  }
  // Set black
  setBlack(raw);

  // Apply opcodes to lossy DNG
  if (compression == 0x884c && !uncorrectedRawValues &&
      raw->hasEntry(OPCODELIST2)) {
    // We must apply black/white scaling
    mRaw->scaleBlackWhite();

    // Apply stage 2 codes
    try {
      DngOpcodes codes(raw->getEntry(OPCODELIST2));
      codes.applyOpCodes(mRaw);
    } catch (RawDecoderException& e) {
      // We push back errors from the opcode parser, since the image may still
      // be usable
      mRaw->setError(e.what());
    }
    mRaw->blackAreas.clear();
    mRaw->blackLevel = 0;
    mRaw->blackLevelSeparate[0] = mRaw->blackLevelSeparate[1] =
        mRaw->blackLevelSeparate[2] = mRaw->blackLevelSeparate[3] = 0;
    mRaw->whitePoint = 65535;
  }

  return mRaw;
}

void DngDecoder::decodeMetaDataInternal(const CameraMetaData* meta) {
  if (mRootIFD->hasEntryRecursive(ISOSPEEDRATINGS))
    mRaw->metadata.isoSpeed = mRootIFD->getEntryRecursive(ISOSPEEDRATINGS)->getU32();

  TiffID id;

  try {
    id = mRootIFD->getID();
  } catch (RawspeedException& e) {
    mRaw->setError(e.what());
    // not all dngs have MAKE/MODEL entries,
    // will be dealt with by using UNIQUECAMERAMODEL below
  }

  // Set the make and model
  mRaw->metadata.make = id.make;
  mRaw->metadata.model = id.model;

  const Camera* cam = meta->getCamera(id.make, id.model, "dng");
  if (!cam) //Also look for non-DNG cameras in case it's a converted file
    cam = meta->getCamera(id.make, id.model, "");
  if (!cam) // Worst case scenario, look for any such camera.
    cam = meta->getCamera(id.make, id.model);
  if (cam) {
    mRaw->metadata.canonical_make = cam->canonical_make;
    mRaw->metadata.canonical_model = cam->canonical_model;
    mRaw->metadata.canonical_alias = cam->canonical_alias;
    mRaw->metadata.canonical_id = cam->canonical_id;
  } else {
    mRaw->metadata.canonical_make = id.make;
    mRaw->metadata.canonical_model = mRaw->metadata.canonical_alias = id.model;
    if (mRootIFD->hasEntryRecursive(UNIQUECAMERAMODEL)) {
      mRaw->metadata.canonical_id = mRootIFD->getEntryRecursive(UNIQUECAMERAMODEL)->getString();
    } else {
      mRaw->metadata.canonical_id = id.make + " " + id.model;
    }
  }

  // Fetch the white balance
  if (mRootIFD->hasEntryRecursive(ASSHOTNEUTRAL)) {
    TiffEntry* as_shot_neutral = mRootIFD->getEntryRecursive(ASSHOTNEUTRAL);
    if (as_shot_neutral->count == 3) {
      for (uint32 i = 0; i < 3; i++) {
        float c = as_shot_neutral->getFloat(i);
        mRaw->metadata.wbCoeffs[i] = (c > 0.0F) ? (1.0F / c) : 0.0F;
      }
    }
  } else if (mRootIFD->hasEntryRecursive(ASSHOTWHITEXY)) {
    TiffEntry* as_shot_white_xy = mRootIFD->getEntryRecursive(ASSHOTWHITEXY);
    if (as_shot_white_xy->count == 2) {
      mRaw->metadata.wbCoeffs[0] = as_shot_white_xy->getFloat(0);
      mRaw->metadata.wbCoeffs[1] = as_shot_white_xy->getFloat(1);
      mRaw->metadata.wbCoeffs[2] =
          1 - mRaw->metadata.wbCoeffs[0] - mRaw->metadata.wbCoeffs[1];

      const float d65_white[3] = {0.950456, 1, 1.088754};
      for (uint32 i = 0; i < 3; i++)
        mRaw->metadata.wbCoeffs[i] /= d65_white[i];
    }
  }
}

/* DNG Images are assumed to be decodable unless explicitly set so */
void DngDecoder::checkSupportInternal(const CameraMetaData* meta) {
  // We set this, since DNG's are not explicitly added.
  failOnUnknown = false;

  if (!(mRootIFD->hasEntryRecursive(MAKE) && mRootIFD->hasEntryRecursive(MODEL))) {
    // Check "Unique Camera Model" instead, uses this for both make + model.
    if (mRootIFD->hasEntryRecursive(UNIQUECAMERAMODEL)) {
      string unique = mRootIFD->getEntryRecursive(UNIQUECAMERAMODEL)->getString();
      checkCameraSupported(meta, {unique, unique}, "dng");
      return;
    }
    // If we don't have make/model we cannot tell, but still assume yes.
    return;
  }

  checkCameraSupported(meta, mRootIFD->getID(), "dng");
}

/* Decodes DNG masked areas into blackareas in the image */
bool DngDecoder::decodeMaskedAreas(const TiffIFD* raw) {
  TiffEntry *masked = raw->getEntry(MASKEDAREAS);

  if (masked->type != TIFF_SHORT && masked->type != TIFF_LONG)
    return false;

  uint32 nrects = masked->count/4;
  if (0 == nrects)
    return false;

  /* Since we may both have short or int, copy it to int array. */
  auto rects = masked->getU32Array(nrects*4);

  iPoint2D top = mRaw->getCropOffset();

  for (uint32 i = 0; i < nrects; i++) {
    iPoint2D topleft = iPoint2D(rects[i * 4UL + 1UL], rects[i * 4UL]);
    iPoint2D bottomright = iPoint2D(rects[i * 4UL + 3UL], rects[i * 4UL + 2UL]);
    // Is this a horizontal box, only add it if it covers the active width of the image
    if (topleft.x <= top.x && bottomright.x >= (mRaw->dim.x + top.x)) {
      mRaw->blackAreas.emplace_back(topleft.y, bottomright.y - topleft.y,
                                    false);
    }
    // Is it a vertical box, only add it if it covers the active height of the
    // image
    else if (topleft.y <= top.y && bottomright.y >= (mRaw->dim.y + top.y)) {
      mRaw->blackAreas.emplace_back(topleft.x, bottomright.x - topleft.x, true);
    }
  }
  return !mRaw->blackAreas.empty();
}

bool DngDecoder::decodeBlackLevels(const TiffIFD* raw) {
  iPoint2D blackdim(1,1);
  if (raw->hasEntry(BLACKLEVELREPEATDIM)) {
    TiffEntry *bleveldim = raw->getEntry(BLACKLEVELREPEATDIM);
    if (bleveldim->count != 2)
      return false;
    blackdim = iPoint2D(bleveldim->getU32(0), bleveldim->getU32(1));
  }

  if (blackdim.x == 0 || blackdim.y == 0)
    return false;

  if (!raw->hasEntry(BLACKLEVEL))
    return true;

  if (mRaw->getCpp() != 1)
    return false;

  TiffEntry* black_entry = raw->getEntry(BLACKLEVEL);
  if (static_cast<int>(black_entry->count) < blackdim.x * blackdim.y)
    ThrowRDE("BLACKLEVEL entry is too small");

  if (blackdim.x < 2 || blackdim.y < 2) {
    // We so not have enough to fill all individually, read a single and copy it
    float value = black_entry->getFloat();
    for (int y = 0; y < 2; y++) {
      for (int x = 0; x < 2; x++)
        mRaw->blackLevelSeparate[y*2+x] = value;
    }
  } else {
    for (int y = 0; y < 2; y++) {
      for (int x = 0; x < 2; x++)
        mRaw->blackLevelSeparate[y*2+x] = black_entry->getFloat(y*blackdim.x+x);
    }
  }

  // DNG Spec says we must add black in deltav and deltah
  if (raw->hasEntry(BLACKLEVELDELTAV)) {
    TiffEntry *blackleveldeltav = raw->getEntry(BLACKLEVELDELTAV);
    if (static_cast<int>(blackleveldeltav->count) < mRaw->dim.y)
      ThrowRDE("BLACKLEVELDELTAV array is too small");
    float black_sum[2] = {0.0F, 0.0F};
    for (int i = 0; i < mRaw->dim.y; i++)
      black_sum[i&1] += blackleveldeltav->getFloat(i);

    for (int i = 0; i < 4; i++)
      mRaw->blackLevelSeparate[i] += static_cast<int>(
          black_sum[i >> 1] / static_cast<float>(mRaw->dim.y) * 2.0F);
  }

  if (raw->hasEntry(BLACKLEVELDELTAH)){
    TiffEntry *blackleveldeltah = raw->getEntry(BLACKLEVELDELTAH);
    if (static_cast<int>(blackleveldeltah->count) < mRaw->dim.x)
      ThrowRDE("BLACKLEVELDELTAH array is too small");
    float black_sum[2] = {0.0F, 0.0F};
    for (int i = 0; i < mRaw->dim.x; i++)
      black_sum[i&1] += blackleveldeltah->getFloat(i);

    for (int i = 0; i < 4; i++)
      mRaw->blackLevelSeparate[i] += static_cast<int>(
          black_sum[i & 1] / static_cast<float>(mRaw->dim.x) * 2.0F);
  }
  return true;
}

void DngDecoder::setBlack(const TiffIFD* raw) {

  if (raw->hasEntry(MASKEDAREAS) && decodeMaskedAreas(raw))
    return;

  // Black defaults to 0
  memset(mRaw->blackLevelSeparate,0,sizeof(mRaw->blackLevelSeparate));

  if (raw->hasEntry(BLACKLEVEL))
    decodeBlackLevels(raw);
}
} // namespace rawspeed
