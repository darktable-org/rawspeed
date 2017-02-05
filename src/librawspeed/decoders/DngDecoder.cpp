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

#include "decoders/DngDecoder.h"
#include "common/Common.h"                // for uint32, uchar8, writeLog
#include "common/DngOpcodes.h"            // for DngOpcodes
#include "common/Point.h"                 // for iPoint2D, iRectangle2D
#include "decoders/DngDecoderSlices.h"    // for DngDecoderSlices, DngSlice...
#include "decoders/RawDecoderException.h" // for ThrowRDE, RawDecoderException
#include "metadata/BlackArea.h"           // for BlackArea
#include "metadata/Camera.h"              // for Camera
#include "metadata/CameraMetaData.h"      // for CameraMetaData
#include "metadata/ColorFilterArray.h"    // for CFAColor, ColorFilterArray
#include "parsers/TiffParserException.h"  // for TiffParserException
#include "tiff/TiffEntry.h"               // for TiffEntry, TiffDataType::T...
#include "tiff/TiffIFD.h"                 // for TiffIFD, TiffRootIFD, TiffID
#include "tiff/TiffTag.h"                 // for TiffTag::UNIQUECAMERAMODEL
#include <algorithm>                      // for move
#include <cstdio>                         // for printf
#include <cstring>                        // for memset
#include <map>                            // for map
#include <memory>                         // for unique_ptr
#include <stdexcept>                      // for out_of_range
#include <string>                         // for string, operator+, basic_s...
#include <vector>                         // for vector, allocator

using namespace std;

namespace RawSpeed {

DngDecoder::DngDecoder(TiffRootIFDOwner&& rootIFD, FileMap* file)
  : AbstractTiffDecoder(move(rootIFD), file)
{
  vector<TiffIFD*> data = mRootIFD->getIFDsWithTag(DNGVERSION);
  const uchar8* v = data[0]->getEntry(DNGVERSION)->getData(4);

  if (v[0] != 1)
    ThrowRDE("Not a supported DNG image format: v%u.%u.%u.%u", (int)v[0], (int)v[1], (int)v[2], (int)v[3]);
//  if (v[1] > 4)
//    ThrowRDE("Not a supported DNG image format: v%u.%u.%u.%u", (int)v[0], (int)v[1], (int)v[2], (int)v[3]);

  if ((v[0] <= 1) && (v[1] < 1))  // Prior to v1.1.xxx  fix LJPEG encoding bug
    mFixLjpeg = true;
  else
    mFixLjpeg = false;
}

void DngDecoder::dropUnsuportedChunks(vector<TiffIFD*>& data) {
  // Erase the ones not with JPEG compression
  for (auto i = data.begin(); i != data.end();) {
    int comp = (*i)->getEntry(COMPRESSION)->getU16();
    bool isSubsampled = false;
    try {
      isSubsampled = (*i)->getEntry(NEWSUBFILETYPE)->getU32() &
                     1; // bit 0 is on if image is subsampled
    } catch (TiffParserException&) {
    }
    if (!(comp == 7 ||
#ifdef HAVE_ZLIB
          comp == 8 ||
#endif
          comp == 1
#ifdef HAVE_ZLIB
          || comp == 0x884c
#endif
          ) ||
        isSubsampled) { // Erase if subsampled, or not deflated, JPEG or
                        // uncompressed
      i = data.erase(i);
    } else {
      ++i;
    }
  }
}

void DngDecoder::parseCFA(TiffIFD* raw) {

  // Check if layout is OK, if present
  if (raw->hasEntry(CFALAYOUT))
    if (raw->getEntry(CFALAYOUT)->getU16() != 1)
      ThrowRDE("DNG Decoder: Unsupported CFA Layout.");

  TiffEntry* cfadim = raw->getEntry(CFAREPEATPATTERNDIM);
  if (cfadim->count != 2)
    ThrowRDE("DNG Decoder: Couldn't read CFA pattern dimension");

  // Does NOT contain dimensions as some documents state
  TiffEntry* cPat = raw->getEntry(CFAPATTERN);

  /*
  if (raw->hasEntry(CFAPLANECOLOR)) {
    // Map from the order in the image, to the position in the CFA
    TiffEntry* e = raw->getEntry(CFAPLANECOLOR);

    const unsigned char* cPlaneOrder = e->getData();
    printf("Planecolor: ");
    for (uint32 i = 0; i < e->count; i++) {
      printf("%u,",cPlaneOrder[i]);
    }
    printf("\n");
  }
  */

  iPoint2D cfaSize(cfadim->getU32(1), cfadim->getU32(0));
  if (cfaSize.area() != cPat->count) {
    ThrowRDE("DNG Decoder: CFA pattern dimension and pattern count does not "
             "match: %d.",
             cPat->count);
  }

  mRaw->cfa.setSize(cfaSize);

  const map<uint32, CFAColor> int2enum = {
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
        ThrowRDE("DNG Decoder: Unsupported CFA Color: %u", c1);
      }

      mRaw->cfa.setColorAt(iPoint2D(x, y), c2);
    }
  }
}

void DngDecoder::decodeData(TiffIFD* raw, int compression) {
  mRaw->createData();

  if (compression == 8 && sample_format != 3) {
    ThrowRDE("DNG Decoder: Only float format is supported for "
             "deflate-compressed data.");
  } else if ((compression == 7 || compression == 0x884c) &&
             sample_format != 1) {
    ThrowRDE("DNG Decoder: Only 16 bit unsigned data supported for "
             "JPEG-compressed data.");
  }

  DngDecoderSlices slices(mFile, mRaw, compression);
  if (raw->hasEntry(PREDICTOR)) {
    uint32 predictor = raw->getEntry(PREDICTOR)->getU32();
    slices.mPredictor = predictor;
  }
  slices.mBps = raw->getEntry(BITSPERSAMPLE)->getU32();
  if (raw->hasEntry(TILEOFFSETS)) {
    uint32 tilew = raw->getEntry(TILEWIDTH)->getU32();
    uint32 tileh = raw->getEntry(TILELENGTH)->getU32();
    if (!tilew || !tileh)
      ThrowRDE("DNG Decoder: Invalid tile size");

    uint32 tilesX = (mRaw->dim.x + tilew - 1) / tilew;
    uint32 tilesY = (mRaw->dim.y + tileh - 1) / tileh;
    uint32 nTiles = tilesX * tilesY;

    TiffEntry* offsets = raw->getEntry(TILEOFFSETS);
    TiffEntry* counts = raw->getEntry(TILEBYTECOUNTS);
    if (offsets->count != counts->count || offsets->count != nTiles) {
      ThrowRDE("DNG Decoder: Tile count mismatch: offsets:%u count:%u, "
               "calculated:%u",
               offsets->count, counts->count, nTiles);
    }

    slices.mFixLjpeg = mFixLjpeg;

    for (uint32 y = 0; y < tilesY; y++) {
      for (uint32 x = 0; x < tilesX; x++) {
        DngSliceElement e(offsets->getU32(x + y * tilesX),
                          counts->getU32(x + y * tilesX), tilew * x, tileh * y,
                          tilew, tileh);
        e.mUseBigtable = tilew * tileh > 1024 * 1024;
        slices.addSlice(e);
      }
    }
  } else { // Strips
    TiffEntry* offsets = raw->getEntry(STRIPOFFSETS);
    TiffEntry* counts = raw->getEntry(STRIPBYTECOUNTS);

    if (counts->count != offsets->count) {
      ThrowRDE("DNG Decoder: Byte count number does not match strip size: "
               "count:%u, stips:%u ",
               counts->count, offsets->count);
    }

    uint32 yPerSlice = raw->hasEntry(ROWSPERSTRIP) ?
          raw->getEntry(ROWSPERSTRIP)->getU32() : mRaw->dim.y;

    if (yPerSlice == 0 || yPerSlice > (uint32)mRaw->dim.y)
      ThrowRDE("DNG Decoder: Invalid y per slice");

    uint32 offY = 0;
    for (uint32 s = 0; s < counts->count; s++) {
      DngSliceElement e(offsets->getU32(s), counts->getU32(s), 0, offY,
                        mRaw->dim.x, yPerSlice);
      e.mUseBigtable = yPerSlice * mRaw->dim.y > 1024 * 1024;
      offY += yPerSlice;

      if (mFile->isValid(e.byteOffset,
                         e.byteCount)) // Only decode if size is valid
        slices.addSlice(e);
    }
  }
  uint32 nSlices = slices.size();
  if (!nSlices)
    ThrowRDE("DNG Decoder: No valid slices found.");

  slices.startDecoding();

  if (mRaw->errors.size() >= nSlices) {
    ThrowRDE(
        "DNG Decoding: Too many errors encountered. Giving up.\nFirst Error:%s",
        mRaw->errors[0]);
  }
}

RawImage DngDecoder::decodeRawInternal() {
  vector<TiffIFD*> data = mRootIFD->getIFDsWithTag(COMPRESSION);

  if (data.empty())
    ThrowRDE("DNG Decoder: No image data found");

  dropUnsuportedChunks(data);

  if (data.empty())
    ThrowRDE("DNG Decoder: No RAW chunks found");

  if (data.size() > 1) {
    writeLog(DEBUG_PRIO_EXTRA, "Multiple RAW chunks found - using first only!");
  }

  TiffIFD* raw = data[0];
  bps = raw->getEntry(BITSPERSAMPLE)->getU32();

  if (raw->hasEntry(SAMPLEFORMAT))
    sample_format = raw->getEntry(SAMPLEFORMAT)->getU32();

  int compression = raw->getEntry(COMPRESSION)->getU16();

  if (sample_format == 1)
    mRaw = RawImage::create(TYPE_USHORT16);
  else if (sample_format == 3)
    mRaw = RawImage::create(TYPE_FLOAT32);
  else
    ThrowRDE("DNG Decoder: Only 16 bit unsigned or float point data supported.");

  mRaw->isCFA = (raw->getEntry(PHOTOMETRICINTERPRETATION)->getU16() == 32803);

  if (mRaw->isCFA)
    writeLog(DEBUG_PRIO_EXTRA, "This is a CFA image\n");
  else {
    writeLog(DEBUG_PRIO_EXTRA, "This is NOT a CFA image\n");
  }

  if (sample_format == 1 && bps > 16)
    ThrowRDE("DNG Decoder: Integer precision larger than 16 bits currently not supported.");

  if (sample_format == 3 && bps != 32 && compression != 8)
    ThrowRDE("DNG Decoder: Uncompressed float point must be 32 bits per sample.");

  try {
    mRaw->dim.x = raw->getEntry(IMAGEWIDTH)->getU32();
    mRaw->dim.y = raw->getEntry(IMAGELENGTH)->getU32();
  } catch (TiffParserException &) {
    ThrowRDE("DNG Decoder: Could not read basic image information.");
  }

  try {

    if (mRaw->isCFA)
      parseCFA(raw);

    uint32 cpp = raw->getEntry(SAMPLESPERPIXEL)->getU32();

    if (cpp > 4)
      ThrowRDE("DNG Decoder: More than 4 samples per pixel is not supported.");

    mRaw->setCpp(cpp);

    // Now load the image
    try {
      decodeData(raw, compression);
    } catch (TiffParserException& e) {
      ThrowRDE("DNG Decoder: Unsupported format, tried strips and tiles:\n%s",
               e.what());
    }
  } catch (TiffParserException &e) {
    ThrowRDE("DNG Decoder: Image could not be read:\n%s", e.what());
  }

  // Fetch the white balance
  if (mRootIFD->hasEntryRecursive(ASSHOTNEUTRAL)) {
    TiffEntry *as_shot_neutral = mRootIFD->getEntryRecursive(ASSHOTNEUTRAL);
    if (as_shot_neutral->count == 3) {
      for (uint32 i=0; i<3; i++)
        mRaw->metadata.wbCoeffs[i] = 1.0f/as_shot_neutral->getFloat(i);
    }
  } else if (mRootIFD->hasEntryRecursive(ASSHOTWHITEXY)) {
    TiffEntry *as_shot_white_xy = mRootIFD->getEntryRecursive(ASSHOTWHITEXY);
    if (as_shot_white_xy->count == 2) {
      mRaw->metadata.wbCoeffs[0] = as_shot_white_xy->getFloat(0);
      mRaw->metadata.wbCoeffs[1] = as_shot_white_xy->getFloat(1);
      mRaw->metadata.wbCoeffs[2] = 1 - mRaw->metadata.wbCoeffs[0] - mRaw->metadata.wbCoeffs[1];

      const float d65_white[3] = { 0.950456, 1, 1.088754 };
      for (uint32 i=0; i<3; i++)
          mRaw->metadata.wbCoeffs[i] /= d65_white[i];
    }
  }

  // Crop
  if (raw->hasEntry(ACTIVEAREA)) {
    iPoint2D new_size(mRaw->dim.x, mRaw->dim.y);

    TiffEntry *active_area = raw->getEntry(ACTIVEAREA);
    if (active_area->count != 4)
      ThrowRDE("DNG: active area has %d values instead of 4", active_area->count);

    auto corners = active_area->getU32Array(4);
    if (iPoint2D(corners[1], corners[0]).isThisInside(mRaw->dim)) {
      if (iPoint2D(corners[3], corners[2]).isThisInside(mRaw->dim)) {
        iRectangle2D crop(corners[1], corners[0], corners[3] - corners[1], corners[2] - corners[0]);
        mRaw->subFrame(crop);
      }
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
      ThrowRDE("DNG Decoder: No positive crop area");

    mRaw->subFrame(cropped);
    if (mRaw->isCFA && cropped.pos.x %2 == 1)
      mRaw->cfa.shiftLeft();
    if (mRaw->isCFA && cropped.pos.y %2 == 1)
      mRaw->cfa.shiftDown();
  }
  if (mRaw->dim.area() <= 0)
    ThrowRDE("DNG Decoder: No image left after crop");

  // Apply stage 1 opcodes
  if (applyStage1DngOpcodes) {
    if (raw->hasEntry(OPCODELIST1))
    {
      // Apply stage 1 codes
      try{
        DngOpcodes codes(raw->getEntry(OPCODELIST1));
        mRaw = codes.applyOpCodes(mRaw);
      } catch (RawDecoderException &e) {
        // We push back errors from the opcode parser, since the image may still be usable
        mRaw->setError(e.what());
      }
    }
  }

  // Linearization
  if (raw->hasEntry(LINEARIZATIONTABLE)) {
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
      auto *pixels = (ushort16 *)mRaw->getData(0, 500);
      float avg = 0.0f;
      for (uint32 x = 0; x < cw; x++) {
        avg += (float)pixels[x];
      }
      printf("Average:%f\n", avg/(float)cw);
    }
  }

 // Default white level is (2 ** BitsPerSample) - 1
  mRaw->whitePoint = (1 >> raw->getEntry(BITSPERSAMPLE)->getU16()) - 1;

  if (raw->hasEntry(WHITELEVEL)) {
    TiffEntry *whitelevel = raw->getEntry(WHITELEVEL);
    if (whitelevel->isInt())
      mRaw->whitePoint = whitelevel->getU32();
  }
  // Set black
  setBlack(raw);

  // Apply opcodes to lossy DNG
  if (compression == 0x884c && !uncorrectedRawValues) {
    if (raw->hasEntry(OPCODELIST2))
    {
      // We must apply black/white scaling
      mRaw->scaleBlackWhite();
      // Apply stage 2 codes
      try{
        DngOpcodes codes(raw->getEntry(OPCODELIST2));
        mRaw = codes.applyOpCodes(mRaw);
      } catch (RawDecoderException &e) {
        // We push back errors from the opcode parser, since the image may still be usable
        mRaw->setError(e.what());
      }
      mRaw->blackAreas.clear();
      mRaw->blackLevel = 0;
      mRaw->blackLevelSeparate[0] = mRaw->blackLevelSeparate[1] = mRaw->blackLevelSeparate[2] = mRaw->blackLevelSeparate[3] = 0;
      mRaw->whitePoint = 65535;
    }
  }

  return mRaw;
}

void DngDecoder::decodeMetaDataInternal(CameraMetaData *meta) {
  if (mRootIFD->hasEntryRecursive(ISOSPEEDRATINGS))
    mRaw->metadata.isoSpeed = mRootIFD->getEntryRecursive(ISOSPEEDRATINGS)->getU32();

  TiffID id;

  try {
    id = mRootIFD->getID();
  } catch (...) {
    // not all dngs have MAKE/MODEL entries,
    // will be dealt with by using UNIQUECAMERAMODEL below
  }

  // Set the make and model
  mRaw->metadata.make = id.make;
  mRaw->metadata.model = id.model;

  Camera *cam = meta->getCamera(id.make, id.model, "dng");
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
}

/* DNG Images are assumed to be decodable unless explicitly set so */
void DngDecoder::checkSupportInternal(CameraMetaData *meta) {
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
bool DngDecoder::decodeMaskedAreas(TiffIFD* raw) {
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
    iPoint2D topleft = iPoint2D(rects[i*4+1], rects[i*4]);
    iPoint2D bottomright = iPoint2D(rects[i*4+3], rects[i*4+2]);
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

bool DngDecoder::decodeBlackLevels(TiffIFD* raw) {
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
  if ((int)black_entry->count < blackdim.x*blackdim.y)
    ThrowRDE("DNG: BLACKLEVEL entry is too small");

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
    if ((int)blackleveldeltav->count < mRaw->dim.y)
      ThrowRDE("DNG: BLACKLEVELDELTAV array is too small");
    float black_sum[2] = {0.0f, 0.0f};
    for (int i = 0; i < mRaw->dim.y; i++)
      black_sum[i&1] += blackleveldeltav->getFloat(i);

    for (int i = 0; i < 4; i++)
      mRaw->blackLevelSeparate[i] += (int)(black_sum[i>>1] / (float)mRaw->dim.y * 2.0f);
  }

  if (raw->hasEntry(BLACKLEVELDELTAH)){
    TiffEntry *blackleveldeltah = raw->getEntry(BLACKLEVELDELTAH);
    if ((int)blackleveldeltah->count < mRaw->dim.x)
      ThrowRDE("DNG: BLACKLEVELDELTAH array is too small");
    float black_sum[2] = {0.0f, 0.0f};
    for (int i = 0; i < mRaw->dim.x; i++)
      black_sum[i&1] += blackleveldeltah->getFloat(i);

    for (int i = 0; i < 4; i++)
      mRaw->blackLevelSeparate[i] += (int)(black_sum[i&1] / (float)mRaw->dim.x * 2.0f);
  }
  return true;
}

void DngDecoder::setBlack(TiffIFD* raw) {

  if (raw->hasEntry(MASKEDAREAS))
    if (decodeMaskedAreas(raw))
      return;

  // Black defaults to 0
  memset(mRaw->blackLevelSeparate,0,sizeof(mRaw->blackLevelSeparate));

  if (raw->hasEntry(BLACKLEVEL))
    decodeBlackLevels(raw);
}
} // namespace RawSpeed
