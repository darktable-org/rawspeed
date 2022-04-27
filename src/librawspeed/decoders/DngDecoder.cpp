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
#include "common/Common.h"                         // for uint32_t, roundUpDi...
#include "common/DngOpcodes.h"                     // for DngOpcodes
#include "common/NORangesSet.h"                    // for set
#include "common/Point.h"                          // for iPoint2D, iRectan...
#include "common/RawspeedException.h"              // for RawspeedException
#include "decoders/RawDecoderException.h"          // for ThrowRDE, RawDeco...
#include "decompressors/AbstractDngDecompressor.h" // for DngSliceElement
#include "io/Buffer.h"                             // for Buffer, DataBuffer
#include "io/ByteStream.h"                         // for ByteStream
#include "metadata/BlackArea.h"                    // for BlackArea
#include "metadata/Camera.h"                       // for Camera
#include "metadata/CameraMetaData.h"               // for CameraMetaData
#include "metadata/ColorFilterArray.h"             // for CFAColor, ColorFi...
#include "parsers/TiffParserException.h"           // for ThrowTPE
#include "tiff/TiffEntry.h" // for TiffEntry, TiffDataType::LONG
#include "tiff/TiffIFD.h"   // for TiffIFD, TiffRootIFD
#include "tiff/TiffTag.h"   // for ACTIVEAREA, TILEO...
#include <algorithm>        // for any_of
#include <array>            // for array, array<>::v...
#include <cassert>          // for assert
#include <limits>           // for numeric_limits
#include <map>              // for map
#include <memory>           // for unique_ptr
#include <stdexcept>        // for out_of_range
#include <string>           // for string, operator+
#include <utility>          // for move, pair
#include <vector>           // for vector, allocator

using std::map;
using std::vector;

namespace rawspeed {

bool __attribute__((pure))
DngDecoder::isAppropriateDecoder(const TiffRootIFD* rootIFD,
                                 [[maybe_unused]] const Buffer& file) {
  return rootIFD->hasEntryRecursive(TiffTag::DNGVERSION);
}

DngDecoder::DngDecoder(TiffRootIFDOwner&& rootIFD, const Buffer& file)
    : AbstractTiffDecoder(std::move(rootIFD), file) {
  if (!mRootIFD->hasEntryRecursive(TiffTag::DNGVERSION))
    ThrowRDE("DNG, but version tag is missing. Will not guess.");

  const uint8_t* v =
      mRootIFD->getEntryRecursive(TiffTag::DNGVERSION)->getData().getData(4);

  if (v[0] != 1)
    ThrowRDE("Not a supported DNG image format: v%u.%u.%u.%u", (int)v[0],
             (int)v[1], (int)v[2], (int)v[3]);
  //  if (v[1] > 4)
  //    ThrowRDE("Not a supported DNG image format: v%u.%u.%u.%u", (int)v[0],
  //    (int)v[1], (int)v[2], (int)v[3]);

  // Prior to v1.1.xxx  fix LJPEG encoding bug
  mFixLjpeg = (v[0] <= 1) && (v[1] < 1);
}

void DngDecoder::dropUnsuportedChunks(std::vector<const TiffIFD*>* data) {
  for (auto i = data->begin(); i != data->end();) {
    const auto& ifd = *i;

    int comp = ifd->getEntry(TiffTag::COMPRESSION)->getU16();
    bool isSubsampled = false;
    bool isAlpha = false;

    if (ifd->hasEntry(TiffTag::NEWSUBFILETYPE) &&
        ifd->getEntry(TiffTag::NEWSUBFILETYPE)->isInt()) {
      const uint32_t NewSubFileType =
          (*i)->getEntry(TiffTag::NEWSUBFILETYPE)->getU32();

      // bit 0 is on if image is subsampled.
      // the value itself can be either 1, or 0x10001.
      // or 5 for "Transparency information for subsampled raw images"
      isSubsampled = NewSubFileType & (1 << 0);

      // bit 2 is on if image contains transparency information.
      // the value itself can be either 4 or 5
      isAlpha = NewSubFileType & (1 << 2);
    }

    // normal raw?
    bool supported = !isSubsampled && !isAlpha;

    switch (comp) {
    case 1: // uncompressed
    case 7: // lossless JPEG
#ifdef HAVE_ZLIB
    case 8: // deflate
#endif
    case 9: // VC-5 as used by GoPro
#ifdef HAVE_JPEG
    case 0x884c: // lossy JPEG
#endif
                 // no change, if supported, then is still supported.
      break;

#ifndef HAVE_ZLIB
    case 8: // deflate
#pragma message                                                                \
    "ZLIB is not present! Deflate compression will not be supported!"
      writeLog(DEBUG_PRIO::WARNING, "DNG Decoder: found Deflate-encoded chunk, "
                                    "but the deflate support was disabled at "
                                    "build!");
      [[clang::fallthrough]];
#endif
#ifndef HAVE_JPEG
    case 0x884c: // lossy JPEG
#pragma message                                                                \
    "JPEG is not present! Lossy JPEG compression will not be supported!"
      writeLog(DEBUG_PRIO::WARNING, "DNG Decoder: found lossy JPEG-encoded "
                                    "chunk, but the jpeg support was "
                                    "disabled at build!");
      [[clang::fallthrough]];
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

void DngDecoder::parseCFA(const TiffIFD* raw, RawImage::frame_ptr_t frame) {

  // Check if layout is OK, if present
  if (raw->hasEntry(TiffTag::CFALAYOUT) &&
      raw->getEntry(TiffTag::CFALAYOUT)->getU16() != 1)
    ThrowRDE("Unsupported CFA Layout.");

  const TiffEntry* cfadim = raw->getEntry(TiffTag::CFAREPEATPATTERNDIM);
  if (cfadim->count != 2)
    ThrowRDE("Couldn't read CFA pattern dimension");

  // Does NOT contain dimensions as some documents state
  const TiffEntry* cPat = raw->getEntry(TiffTag::CFAPATTERN);

  iPoint2D cfaSize(cfadim->getU32(1), cfadim->getU32(0));
  if (cfaSize.area() != cPat->count) {
    ThrowRDE("CFA pattern dimension and pattern count does not "
             "match: %d.",
             cPat->count);
  }

  frame->cfa.setSize(cfaSize);

  static const map<uint32_t, CFAColor> int2enum = {
      {0, CFAColor::RED},   {1, CFAColor::GREEN},   {2, CFAColor::BLUE},
      {3, CFAColor::CYAN},  {4, CFAColor::MAGENTA}, {5, CFAColor::YELLOW},
      {6, CFAColor::WHITE},
  };

  for (int y = 0; y < cfaSize.y; y++) {
    for (int x = 0; x < cfaSize.x; x++) {
      uint32_t c1 = cPat->getByte(x + y * cfaSize.x);
      CFAColor c2 = CFAColor::UNKNOWN;

      try {
        c2 = int2enum.at(c1);
      } catch (const std::out_of_range&) {
        ThrowRDE("Unsupported CFA Color: %u", c1);
      }

      frame->cfa.setColorAt(iPoint2D(x, y), c2);
    }
  }

  // the cfa is specified relative to the ActiveArea. we want it relative (0,0)
  // Since in handleMetadata(), in subFrame() we unconditionally shift CFA by
  // activearea+DefaultCropOrigin; here we need to undo the 'ACTIVEAREA' part.
  if (!raw->hasEntry(TiffTag::ACTIVEAREA))
    return;

  const TiffEntry* active_area = raw->getEntry(TiffTag::ACTIVEAREA);
  if (active_area->count != 4)
    ThrowRDE("active area has %d values instead of 4", active_area->count);

  const auto aa = active_area->getFloatArray(2);
  if (std::any_of(aa.cbegin(), aa.cend(), [](const auto v) {
        return v < std::numeric_limits<iPoint2D::value_type>::min() ||
               v > std::numeric_limits<iPoint2D::value_type>::max();
      }))
    ThrowRDE("Error decoding active area");

  frame->cfa.shiftLeft(aa[1]);
  frame->cfa.shiftDown(aa[0]);
}

DngTilingDescription
DngDecoder::getTilingDescription(const TiffIFD* raw,
                                 RawImage::frame_ptr_t frame) {
  if (raw->hasEntry(TiffTag::TILEOFFSETS)) {
    const uint32_t tilew = raw->getEntry(TiffTag::TILEWIDTH)->getU32();
    const uint32_t tileh = raw->getEntry(TiffTag::TILELENGTH)->getU32();

    if (!(tilew > 0 && tileh > 0))
      ThrowRDE("Invalid tile size: (%u, %u)", tilew, tileh);

    assert(tilew > 0);
    const uint32_t tilesX = roundUpDivision(frame->dim.x, tilew);
    if (!tilesX)
      ThrowRDE("Zero tiles horizontally");

    assert(tileh > 0);
    const uint32_t tilesY = roundUpDivision(frame->dim.y, tileh);
    if (!tilesY)
      ThrowRDE("Zero tiles vertically");

    const TiffEntry* offsets = raw->getEntry(TiffTag::TILEOFFSETS);
    if (const TiffEntry* counts = raw->getEntry(TiffTag::TILEBYTECOUNTS);
        offsets->count != counts->count) {
      ThrowRDE("Tile count mismatch: offsets:%u count:%u", offsets->count,
               counts->count);
    }

    // tilesX * tilesY may overflow, but division is fine, so let's do that.
    if ((offsets->count / tilesX != tilesY || (offsets->count % tilesX != 0)) ||
        (offsets->count / tilesY != tilesX || (offsets->count % tilesY != 0))) {
      ThrowRDE("Tile X/Y count mismatch: total:%u X:%u, Y:%u", offsets->count,
               tilesX, tilesY);
    }

    return {frame->dim, tilew, tileh};
  }

  // Strips
  const TiffEntry* offsets = raw->getEntry(TiffTag::STRIPOFFSETS);
  const TiffEntry* counts = raw->getEntry(TiffTag::STRIPBYTECOUNTS);

  if (counts->count != offsets->count) {
    ThrowRDE("Byte count number does not match strip size: "
             "count:%u, stips:%u ",
             counts->count, offsets->count);
  }

  uint32_t yPerSlice = raw->hasEntry(TiffTag::ROWSPERSTRIP)
                           ? raw->getEntry(TiffTag::ROWSPERSTRIP)->getU32()
                           : frame->dim.y;

  if (yPerSlice == 0 || yPerSlice > static_cast<uint32_t>(frame->dim.y) ||
      roundUpDivision(frame->dim.y, yPerSlice) != counts->count) {
    ThrowRDE("Invalid y per slice %u or strip count %u (height = %u)",
             yPerSlice, counts->count, frame->dim.y);
  }

  return {frame->dim, static_cast<uint32_t>(frame->dim.x), yPerSlice};
}

void DngDecoder::decodeData(const TiffIFD* raw, uint32_t sample_format,
                            int compression, int bps,
                            RawImage::frame_ptr_t frame) {
  if (compression == 8 && sample_format != 3) {
    ThrowRDE("Only float format is supported for "
             "deflate-compressed data.");
  } else if ((compression == 7 || compression == 0x884c) &&
             sample_format != 1) {
    ThrowRDE("Only 16 bit unsigned data supported for "
             "JPEG-compressed data.");
  }

  uint32_t predictor = ~0U;
  if (raw->hasEntry(TiffTag::PREDICTOR))
    predictor = raw->getEntry(TiffTag::PREDICTOR)->getU32();

  // Some decompressors (such as VC5) may depend on the white point
  if (raw->hasEntry(TiffTag::WHITELEVEL)) {
    const TiffEntry* whitelevel = raw->getEntry(TiffTag::WHITELEVEL);
    if (whitelevel->isInt())
      frame->whitePoint = whitelevel->getU32();
  }

  AbstractDngDecompressor slices(frame.get(), getTilingDescription(raw, frame),
                                 compression, mFixLjpeg, bps, predictor);

  slices.slices.reserve(slices.dsc.numTiles);

  const TiffEntry* offsets = nullptr;
  const TiffEntry* counts = nullptr;
  if (raw->hasEntry(TiffTag::TILEOFFSETS)) {
    offsets = raw->getEntry(TiffTag::TILEOFFSETS);
    counts = raw->getEntry(TiffTag::TILEBYTECOUNTS);
  } else { // Strips
    offsets = raw->getEntry(TiffTag::STRIPOFFSETS);
    counts = raw->getEntry(TiffTag::STRIPBYTECOUNTS);
  }
  assert(slices.dsc.numTiles == offsets->count);
  assert(slices.dsc.numTiles == counts->count);

  NORangesSet<Buffer> tilesLegality;
  for (auto n = 0U; n < slices.dsc.numTiles; n++) {
    const auto offset = offsets->getU32(n);
    const auto count = counts->getU32(n);

    if (count < 1)
      ThrowRDE("Tile %u is empty", n);

    ByteStream bs(DataBuffer(mFile.getSubView(offset, count),
                             mRootIFD->rootBuffer.getByteOrder()));

    if (!tilesLegality.insert(bs))
      ThrowTPE("Two tiles overlap. Raw corrupt!");

    slices.slices.emplace_back(slices.dsc, n, bs);
  }

  assert(slices.slices.size() == slices.dsc.numTiles);
  if (slices.slices.empty())
    ThrowRDE("No valid slices found.");

  // FIXME: should we sort the tiles, to linearize the input reading?

  frame->createData();

  slices.decompress();
}

void DngDecoder::decodeRawInternal() {
  vector<const TiffIFD*> data = mRootIFD->getIFDsWithTag(TiffTag::COMPRESSION);

  if (data.empty())
    ThrowRDE("No image data found");

  dropUnsuportedChunks(&data);

  if (data.empty())
    ThrowRDE("No RAW chunks found");

  mRaw.clear();

  /// TODO paralelize?
  for (const auto* raw : data) {
    int bps = raw->getEntry(TiffTag::BITSPERSAMPLE)->getU32();
    if (bps < 1 || bps > 32)
      ThrowRDE("Unsupported bit per sample count: %u.", bps);

    uint32_t sample_format = 1;
    if (raw->hasEntry(TiffTag::SAMPLEFORMAT))
      sample_format = raw->getEntry(TiffTag::SAMPLEFORMAT)->getU32();

    int compression = raw->getEntry(TiffTag::COMPRESSION)->getU16();

    std::shared_ptr<RawImageData> frame;

    switch (sample_format) {
    case 1:
      frame = std::make_shared<RawImageDataU16>();
      break;
    case 3:
      frame = std::make_shared<RawImageDataFloat>();
      break;
    default:
      ThrowRDE("Only 16 bit unsigned or float point data supported. Sample "
               "format %u is not supported.",
               sample_format);
    }

    frame->isCFA =
        (raw->getEntry(TiffTag::PHOTOMETRICINTERPRETATION)->getU16() == 32803);

    if (frame->isCFA)
      writeLog(DEBUG_PRIO::EXTRA, "This is a CFA image");
    else {
      writeLog(DEBUG_PRIO::EXTRA, "This is NOT a CFA image");
    }

    if (sample_format == 1 && bps > 16)
      ThrowRDE(
          "Integer precision larger than 16 bits currently not supported.");

    if (sample_format == 3 && bps != 16 && bps != 24 && bps != 32)
      ThrowRDE("Floating point must be 16/24/32 bits per sample.");

    frame->dim.x = raw->getEntry(TiffTag::IMAGEWIDTH)->getU32();
    frame->dim.y = raw->getEntry(TiffTag::IMAGELENGTH)->getU32();

    if (!frame->dim.hasPositiveArea())
      ThrowRDE("Image has zero size");

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    // Yeah, sure, here it would be just dumb to leave this for production :)
    if (mRaw.get(0)->dim.x > 7424 || mRaw.get(0)->dim.y > 5552) {
      ThrowRDE("Unexpected image dimensions found: (%u; %u)",
               mRaw.get(0)->dim.x, mRaw.get(0)->dim.y);
    }
#endif

    if (frame->isCFA)
      parseCFA(raw, frame);

    uint32_t cpp = raw->getEntry(TiffTag::SAMPLESPERPIXEL)->getU32();

    if (cpp < 1 || cpp > 4)
      ThrowRDE("Unsupported samples per pixel count: %u.", cpp);

    frame->setCpp(cpp);

    // Now load the image
    decodeData(raw, sample_format, compression, bps, frame);

    handleMetadata(raw, compression, bps, frame);
    mRaw.appendFrame(frame);
  }
}

void DngDecoder::handleMetadata(const TiffIFD* raw, int compression, int bps,
                                RawImage::frame_ptr_t frame) {
  // Crop
  if (raw->hasEntry(TiffTag::ACTIVEAREA)) {
    const TiffEntry* active_area = raw->getEntry(TiffTag::ACTIVEAREA);
    if (active_area->count != 4)
      ThrowRDE("active area has %d values instead of 4", active_area->count);

    const iRectangle2D fullImage(0, 0, frame->dim.x, frame->dim.y);

    const auto corners = active_area->getU32Array(4);
    const iPoint2D topLeft(corners[1], corners[0]);
    const iPoint2D bottomRight(corners[3], corners[2]);

    if (!(fullImage.isPointInsideInclusive(topLeft) &&
          fullImage.isPointInsideInclusive(bottomRight) &&
          bottomRight >= topLeft)) {
      ThrowRDE("Rectangle (%u, %u, %u, %u) not inside image (%u, %u, %u, %u).",
               topLeft.x, topLeft.y, bottomRight.x, bottomRight.y,
               fullImage.getTopLeft().x, fullImage.getTopLeft().y,
               fullImage.getBottomRight().x, fullImage.getBottomRight().y);
    }

    iRectangle2D crop;
    crop.setTopLeft(topLeft);
    crop.setBottomRightAbsolute(bottomRight);
    assert(fullImage.isThisInside(fullImage));

    frame->subFrame(crop);
  }

  if (raw->hasEntry(TiffTag::DEFAULTCROPORIGIN) &&
      raw->hasEntry(TiffTag::DEFAULTCROPSIZE)) {
    iRectangle2D cropped(0, 0, frame->dim.x, frame->dim.y);
    const TiffEntry* origin_entry = raw->getEntry(TiffTag::DEFAULTCROPORIGIN);
    const TiffEntry* size_entry = raw->getEntry(TiffTag::DEFAULTCROPSIZE);

    /* Read crop position (sometimes is rational so use float) */
    const auto tl = origin_entry->getFloatArray(2);
    if (std::any_of(tl.cbegin(), tl.cend(), [](const auto v) {
          return v < std::numeric_limits<iPoint2D::value_type>::min() ||
                 v > std::numeric_limits<iPoint2D::value_type>::max();
        }))
      ThrowRDE("Error decoding default crop origin");

    if (iPoint2D cropOrigin(tl[0], tl[1]);
        cropped.isPointInsideInclusive(cropOrigin))
      cropped = iRectangle2D(cropOrigin, {0, 0});

    cropped.dim = frame->dim - cropped.pos;

    /* Read size (sometimes is rational so use float) */
    const auto sz = size_entry->getFloatArray(2);
    if (std::any_of(sz.cbegin(), sz.cend(), [](const auto v) {
          return v < std::numeric_limits<iPoint2D::value_type>::min() ||
                 v > std::numeric_limits<iPoint2D::value_type>::max();
        }))
      ThrowRDE("Error decoding default crop size");

    if (iPoint2D size(sz[0], sz[1]);
        size.isThisInside(frame->dim) &&
        (size + cropped.pos).isThisInside(frame->dim))
      cropped.dim = size;

    if (!cropped.hasPositiveArea())
      ThrowRDE("No positive crop area");

    frame->subFrame(cropped);
  }
  if (frame->dim.area() <= 0)
    ThrowRDE("No image left after crop");

  // Apply stage 1 opcodes
  if (applyStage1DngOpcodes && raw->hasEntry(TiffTag::OPCODELIST1)) {
    try {
      const TiffEntry* opcodes = raw->getEntry(TiffTag::OPCODELIST1);
      // The entry might exist, but it might be empty, which means no opcodes
      if (opcodes->count > 0) {
        DngOpcodes codes(frame.get(), opcodes);
        codes.applyOpCodes(frame.get());
      }
    } catch (const RawDecoderException& e) {
      // We push back errors from the opcode parser, since the image may still
      // be usable
      frame->setError(e.what());
    }
  }

  // Linearization
  if (raw->hasEntry(TiffTag::LINEARIZATIONTABLE) &&
      raw->getEntry(TiffTag::LINEARIZATIONTABLE)->count > 0) {
    const TiffEntry* lintable = raw->getEntry(TiffTag::LINEARIZATIONTABLE);
    auto table = lintable->getU16Array(lintable->count);
    RawImageCurveGuard curveHandler(frame.get(), table, uncorrectedRawValues);
    if (!uncorrectedRawValues)
      frame->sixteenBitLookup();
  }

  if (frame->getDataType() == RawImageType::UINT16) {
    // Default white level is (2 ** BitsPerSample) - 1
    frame->whitePoint = (1UL << bps) - 1UL;
  } else if (frame->getDataType() == RawImageType::F32) {
    // Default white level is 1.0f. But we can't represent that here.
    frame->whitePoint = 65535;
  }

  if (raw->hasEntry(TiffTag::WHITELEVEL)) {
    const TiffEntry* whitelevel = raw->getEntry(TiffTag::WHITELEVEL);
    if (whitelevel->isInt())
      frame->whitePoint = whitelevel->getU32();
  }
  // Set black
  setBlack(raw, frame);

  // Apply opcodes to lossy DNG
  if (compression == 0x884c && !uncorrectedRawValues &&
      raw->hasEntry(TiffTag::OPCODELIST2)) {
    // We must apply black/white scaling
    frame->scaleBlackWhite();

    // Apply stage 2 codes
    try {
      DngOpcodes codes(frame.get(), raw->getEntry(TiffTag::OPCODELIST2));
      codes.applyOpCodes(frame.get());
    } catch (const RawDecoderException& e) {
      // We push back errors from the opcode parser, since the image may still
      // be usable
      frame->setError(e.what());
    }
    frame->blackAreas.clear();
    frame->blackLevel = 0;
    frame->blackLevelSeparate[0] = frame->blackLevelSeparate[1] =
        frame->blackLevelSeparate[2] = frame->blackLevelSeparate[3] = 0;
    frame->whitePoint = 65535;
  }
}

void DngDecoder::decodeMetaDataInternal(const CameraMetaData* meta) {
  if (mRootIFD->hasEntryRecursive(TiffTag::ISOSPEEDRATINGS))
    mRaw.metadata.isoSpeed =
        mRootIFD->getEntryRecursive(TiffTag::ISOSPEEDRATINGS)->getU32();

  TiffID id;

  try {
    id = mRootIFD->getID();
  } catch (const RawspeedException& e) {
    mRaw.get(0)->setError(e.what());
    // not all dngs have MAKE/MODEL entries,
    // will be dealt with by using UNIQUECAMERAMODEL below
  }

  // Set the make and model
  mRaw.metadata.make = id.make;
  mRaw.metadata.model = id.model;

  const Camera* cam = meta->getCamera(id.make, id.model, "dng");
  if (!cam) // Also look for non-DNG cameras in case it's a converted file
    cam = meta->getCamera(id.make, id.model, "");
  if (!cam) // Worst case scenario, look for any such camera.
    cam = meta->getCamera(id.make, id.model);
  if (cam) {
    mRaw.metadata.canonical_make = cam->canonical_make;
    mRaw.metadata.canonical_model = cam->canonical_model;
    mRaw.metadata.canonical_alias = cam->canonical_alias;
    mRaw.metadata.canonical_id = cam->canonical_id;
  } else {
    mRaw.metadata.canonical_make = id.make;
    mRaw.metadata.canonical_model = mRaw.metadata.canonical_alias = id.model;
    if (mRootIFD->hasEntryRecursive(TiffTag::UNIQUECAMERAMODEL)) {
      mRaw.metadata.canonical_id =
          mRootIFD->getEntryRecursive(TiffTag::UNIQUECAMERAMODEL)->getString();
    } else {
      mRaw.metadata.canonical_id = id.make + " " + id.model;
    }
  }

  // Fetch the white balance
  if (mRootIFD->hasEntryRecursive(TiffTag::ASSHOTNEUTRAL)) {
    const TiffEntry* as_shot_neutral =
        mRootIFD->getEntryRecursive(TiffTag::ASSHOTNEUTRAL);
    if (as_shot_neutral->count == 3) {
      for (uint32_t i = 0; i < 3; i++) {
        float c = as_shot_neutral->getFloat(i);
        mRaw.metadata.wbCoeffs[i] = (c > 0.0F) ? (1.0F / c) : 0.0F;
      }
    }
  } else if (mRootIFD->hasEntryRecursive(TiffTag::ASSHOTWHITEXY)) {
    const TiffEntry* as_shot_white_xy =
        mRootIFD->getEntryRecursive(TiffTag::ASSHOTWHITEXY);
    if (as_shot_white_xy->count == 2) {
      mRaw.metadata.wbCoeffs[0] = as_shot_white_xy->getFloat(0);
      mRaw.metadata.wbCoeffs[1] = as_shot_white_xy->getFloat(1);
      mRaw.metadata.wbCoeffs[2] =
          1 - mRaw.metadata.wbCoeffs[0] - mRaw.metadata.wbCoeffs[1];

      const std::array<float, 3> d65_white = {{0.950456, 1, 1.088754}};
      for (uint32_t i = 0; i < 3; i++)
        mRaw.metadata.wbCoeffs[i] /= d65_white[i];
    }
  }

  if (mRootIFD->hasEntryRecursive(TiffTag::COLORMATRIX2) &&
      mRootIFD->hasEntryRecursive(TiffTag::CALIBRATIONILLUMINANT2)) {
    const TiffEntry* illuminant =
        mRootIFD->getEntryRecursive(TiffTag::CALIBRATIONILLUMINANT2);
    if (illuminant->getU16() == 21) { // D65
      const TiffEntry* mat = mRootIFD->getEntryRecursive(TiffTag::COLORMATRIX2);
      const auto srat_vals = mat->getSRationalArray(mat->count);
      bool Success = true;
      mRaw.metadata.colorMatrix.reserve(mat->count);
      for (const auto& val : srat_vals) {
        // FIXME: introduce proper rational type.
        Success &= val.second == 10'000;
        if (!Success)
          break;
        mRaw.metadata.colorMatrix.emplace_back(val.first);
      }
      if (!Success)
        mRaw.metadata.colorMatrix.clear();
    }
  }
}

/* DNG Images are assumed to be decodable unless explicitly set so */
void DngDecoder::checkSupportInternal(const CameraMetaData* meta) {
  // We set this, since DNG's are not explicitly added.
  failOnUnknown = false;

  if (!(mRootIFD->hasEntryRecursive(TiffTag::MAKE) &&
        mRootIFD->hasEntryRecursive(TiffTag::MODEL))) {
    // Check "Unique Camera Model" instead, uses this for both make + model.
    if (mRootIFD->hasEntryRecursive(TiffTag::UNIQUECAMERAMODEL)) {
      std::string unique =
          mRootIFD->getEntryRecursive(TiffTag::UNIQUECAMERAMODEL)->getString();
      checkCameraSupported(meta, {unique, unique}, "dng");
      return;
    }
    // If we don't have make/model we cannot tell, but still assume yes.
    return;
  }

  checkCameraSupported(meta, mRootIFD->getID(), "dng");
}

/* Decodes DNG masked areas into blackareas in the image */
bool DngDecoder::decodeMaskedAreas(const TiffIFD* raw,
                                   RawImage::frame_ptr_t frame) {
  const TiffEntry* masked = raw->getEntry(TiffTag::MASKEDAREAS);

  if (masked->type != TiffDataType::SHORT && masked->type != TiffDataType::LONG)
    return false;

  uint32_t nrects = masked->count / 4;
  if (0 == nrects)
    return false;

  /* Since we may both have short or int, copy it to int array. */
  auto rects = masked->getU32Array(nrects * 4);

  const iRectangle2D fullImage(0, 0, frame->getUncroppedDim().x,
                               frame->getUncroppedDim().y);
  const iPoint2D top = frame->getCropOffset();

  for (uint32_t i = 0; i < nrects; i++) {
    iPoint2D topleft(rects[i * 4UL + 1UL], rects[i * 4UL]);
    iPoint2D bottomright(rects[i * 4UL + 3UL], rects[i * 4UL + 2UL]);

    if (!(fullImage.isPointInsideInclusive(topleft) &&
          fullImage.isPointInsideInclusive(bottomright) &&
          (topleft < bottomright)))
      ThrowRDE("Bad masked area.");

    // Is this a horizontal box, only add it if it covers the active width of
    // the image
    if (topleft.x <= top.x && bottomright.x >= (frame->dim.x + top.x)) {
      frame->blackAreas.emplace_back(topleft.y, bottomright.y - topleft.y,
                                     false);
    }
    // Is it a vertical box, only add it if it covers the active height of the
    // image
    else if (topleft.y <= top.y && bottomright.y >= (frame->dim.y + top.y)) {
      frame->blackAreas.emplace_back(topleft.x, bottomright.x - topleft.x,
                                     true);
    }
  }
  return !frame->blackAreas.empty();
}

bool DngDecoder::decodeBlackLevels(const TiffIFD* raw,
                                   RawImage::frame_ptr_t frame) {
  iPoint2D blackdim(1, 1);
  if (raw->hasEntry(TiffTag::BLACKLEVELREPEATDIM)) {
    const TiffEntry* bleveldim = raw->getEntry(TiffTag::BLACKLEVELREPEATDIM);
    if (bleveldim->count != 2)
      return false;
    blackdim = iPoint2D(bleveldim->getU32(0), bleveldim->getU32(1));
  }

  if (blackdim.x == 0 || blackdim.y == 0)
    return false;

  if (!raw->hasEntry(TiffTag::BLACKLEVEL))
    return true;

  if (frame->getCpp() != 1)
    return false;

  const TiffEntry* black_entry = raw->getEntry(TiffTag::BLACKLEVEL);
  if (black_entry->count < blackdim.area())
    ThrowRDE("BLACKLEVEL entry is too small");

  using BlackType = decltype(mRaw.get(0)->blackLevelSeparate)::value_type;

  if (blackdim.x < 2 || blackdim.y < 2) {
    // We so not have enough to fill all individually, read a single and copy it
    float value = black_entry->getFloat();

    if (value < std::numeric_limits<BlackType>::min() ||
        value > std::numeric_limits<BlackType>::max())
      ThrowRDE("Error decoding black level");

    for (int y = 0; y < 2; y++) {
      for (int x = 0; x < 2; x++)
        frame->blackLevelSeparate[y * 2 + x] = value;
    }
  } else {
    for (int y = 0; y < 2; y++) {
      for (int x = 0; x < 2; x++) {
        float value = black_entry->getFloat(y * blackdim.x + x);

        if (value < std::numeric_limits<BlackType>::min() ||
            value > std::numeric_limits<BlackType>::max())
          ThrowRDE("Error decoding black level");

        frame->blackLevelSeparate[y * 2 + x] = value;
      }
    }
  }

  // DNG Spec says we must add black in deltav and deltah
  if (raw->hasEntry(TiffTag::BLACKLEVELDELTAV)) {
    const TiffEntry* blackleveldeltav =
        raw->getEntry(TiffTag::BLACKLEVELDELTAV);
    if (static_cast<int>(blackleveldeltav->count) < frame->dim.y)
      ThrowRDE("BLACKLEVELDELTAV array is too small");
    std::array<float, 2> black_sum = {{}};
    for (int i = 0; i < frame->dim.y; i++)
      black_sum[i & 1] += blackleveldeltav->getFloat(i);

    for (int i = 0; i < 4; i++) {
      const float value =
          black_sum[i >> 1] / static_cast<float>(frame->dim.y) * 2.0F;
      if (value < std::numeric_limits<BlackType>::min() ||
          value > std::numeric_limits<BlackType>::max())
        ThrowRDE("Error decoding black level");

      if (__builtin_sadd_overflow(frame->blackLevelSeparate[i], value,
                                  &frame->blackLevelSeparate[i]))
        ThrowRDE("Integer overflow when calculating black level");
    }
  }

  if (raw->hasEntry(TiffTag::BLACKLEVELDELTAH)) {
    const TiffEntry* blackleveldeltah =
        raw->getEntry(TiffTag::BLACKLEVELDELTAH);
    if (static_cast<int>(blackleveldeltah->count) < frame->dim.x)
      ThrowRDE("BLACKLEVELDELTAH array is too small");
    std::array<float, 2> black_sum = {{}};
    for (int i = 0; i < frame->dim.x; i++)
      black_sum[i & 1] += blackleveldeltah->getFloat(i);

    for (int i = 0; i < 4; i++) {
      const float value =
          black_sum[i & 1] / static_cast<float>(frame->dim.x) * 2.0F;
      if (value < std::numeric_limits<BlackType>::min() ||
          value > std::numeric_limits<BlackType>::max())
        ThrowRDE("Error decoding black level");

      if (__builtin_sadd_overflow(frame->blackLevelSeparate[i], value,
                                  &frame->blackLevelSeparate[i]))
        ThrowRDE("Integer overflow when calculating black level");
    }
  }
  return true;
}

void DngDecoder::setBlack(const TiffIFD* raw, RawImage::frame_ptr_t frame) {

  if (raw->hasEntry(TiffTag::MASKEDAREAS) && decodeMaskedAreas(raw, frame))
    return;

  // Black defaults to 0
  frame->blackLevelSeparate.fill(0);

  if (raw->hasEntry(TiffTag::BLACKLEVEL))
    decodeBlackLevels(raw, frame);
}
} // namespace rawspeed
