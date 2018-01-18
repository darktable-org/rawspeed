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

#include "rawspeedconfig.h"
#include "decoders/RawDecoder.h"
#include "common/Common.h"                          // for uint32, splitString
#include "common/Point.h"                           // for iPoint2D, iRecta...
#include "decoders/RawDecoderException.h"           // for ThrowRDE, RawDec...
#include "decompressors/UncompressedDecompressor.h" // for UncompressedDeco...
#include "io/Buffer.h"                              // for Buffer
#include "io/FileIOException.h"                     // for FileIOException
#include "io/IOException.h"                         // for IOException
#include "metadata/Camera.h"                        // for Camera, Hints
#include "metadata/CameraMetaData.h"                // for CameraMetaData
#include "metadata/CameraSensorInfo.h"              // for CameraSensorInfo
#include "metadata/ColorFilterArray.h"              // for ColorFilterArray
#include "parsers/TiffParserException.h"            // for TiffParserException
#include "tiff/TiffEntry.h"                         // for TiffEntry
#include "tiff/TiffIFD.h"                           // for TiffIFD
#include "tiff/TiffTag.h"                           // for TiffTag::STRIPOF...
#include <string>                                   // for string, basic_st...
#include <vector>                                   // for vector

using std::vector;
using std::string;

namespace rawspeed {

RawDecoder::RawDecoder(const Buffer* file)
    : mRaw(RawImage::create()), mFile(file) {
  failOnUnknown = false;
  interpolateBadPixels = true;
  applyStage1DngOpcodes = true;
  applyCrop = true;
  uncorrectedRawValues = false;
  fujiRotate = true;
}

void RawDecoder::decodeUncompressed(const TiffIFD *rawIFD, BitOrder order) {
  TiffEntry *offsets = rawIFD->getEntry(STRIPOFFSETS);
  TiffEntry *counts = rawIFD->getEntry(STRIPBYTECOUNTS);
  uint32 yPerSlice = rawIFD->getEntry(ROWSPERSTRIP)->getU32();
  uint32 width = rawIFD->getEntry(IMAGEWIDTH)->getU32();
  uint32 height = rawIFD->getEntry(IMAGELENGTH)->getU32();
  uint32 bitPerPixel = rawIFD->getEntry(BITSPERSAMPLE)->getU32();

  if (width == 0 || height == 0 || width > 5632 || height > 3720)
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", width, height);

  mRaw->dim = iPoint2D(width, height);

  if (counts->count != offsets->count) {
    ThrowRDE("Byte count number does not match strip size: "
             "count:%u, stips:%u ",
             counts->count, offsets->count);
  }

  if (yPerSlice == 0 || yPerSlice > static_cast<uint32>(mRaw->dim.y) ||
      roundUpDivision(mRaw->dim.y, yPerSlice) != counts->count) {
    ThrowRDE("Invalid y per slice %u or strip count %u (height = %u)",
             yPerSlice, counts->count, mRaw->dim.y);
  }

  switch (bitPerPixel) {
  case 12:
  case 14:
    break;
  default:
    ThrowRDE("Unexpected bits per pixel: %u.", bitPerPixel);
  };

  vector<RawSlice> slices;
  slices.reserve(counts->count);
  uint32 offY = 0;

  for (uint32 s = 0; s < counts->count; s++) {
    RawSlice slice;
    slice.offset = offsets->getU32(s);
    slice.count = counts->getU32(s);

    if (slice.count < 1)
      ThrowRDE("Slice %u is empty", s);

    if (offY + yPerSlice > height)
      slice.h = height - offY;
    else
      slice.h = yPerSlice;

    offY += yPerSlice;

    if (!mFile->isValid(slice.offset, slice.count))
      ThrowRDE("Slice offset/count invalid");

    slices.push_back(slice);
  }

  if (slices.empty())
    ThrowRDE("No valid slices found. File probably truncated.");

  assert(height <= offY);
  assert(slices.size() == counts->count);

  mRaw->createData();

  // Default white level is (2 ** BitsPerSample) - 1
  mRaw->whitePoint = (1UL << bitPerPixel) - 1UL;

  offY = 0;
  for (uint32 i = 0; i < slices.size(); i++) {
    RawSlice slice = slices[i];
    UncompressedDecompressor u(*mFile, slice.offset, slice.count, mRaw);
    iPoint2D size(width, slice.h);
    iPoint2D pos(0, offY);
    bitPerPixel = static_cast<int>(
        static_cast<uint64>(static_cast<uint64>(slice.count) * 8U) /
        (slice.h * width));
    const auto inputPitch = width * bitPerPixel / 8;
    if (!inputPitch)
      ThrowRDE("Bad input pitch. Can not decode anything.");
    try {
      u.readUncompressedRaw(size, pos, inputPitch, bitPerPixel, order);
    } catch (RawDecoderException &e) {
      if (i>0)
        mRaw->setError(e.what());
      else
        throw;
    } catch (IOException &e) {
      if (i>0)
        mRaw->setError(e.what());
      else {
        ThrowRDE("IO error occurred in first slice, unable to decode more. "
                 "Error is: %s",
                 e.what());
      }
    }
    offY += slice.h;
  }
}

void RawDecoder::askForSamples(const CameraMetaData* meta, const string& make,
                               const string& model, const string& mode) const {
  if ("dng" == mode)
    return;

  writeLog(DEBUG_PRIO_WARNING,
           "Unable to find camera in database: '%s' '%s' "
           "'%s'\nPlease consider providing samples on "
           "<https://raw.pixls.us/>, thanks!",
           make.c_str(), model.c_str(), mode.c_str());
}

bool RawDecoder::checkCameraSupported(const CameraMetaData* meta,
                                      const string& make, const string& model,
                                      const string& mode) {
  mRaw->metadata.make = make;
  mRaw->metadata.model = model;
  const Camera* cam = meta->getCamera(make, model, mode);
  if (!cam) {
    askForSamples(meta, make, model, mode);

    if (failOnUnknown)
      ThrowRDE("Camera '%s' '%s', mode '%s' not supported, and not allowed to guess. Sorry.", make.c_str(), model.c_str(), mode.c_str());

    // Assume the camera can be decoded, but return false, so decoders can see that we are unsure.
    return false;
  }

  if (!cam->supported)
    ThrowRDE("Camera not supported (explicit). Sorry.");

  if (cam->decoderVersion > getDecoderVersion())
    ThrowRDE("Camera not supported in this version. Update RawSpeed for support.");

  hints = cam->hints;
  return true;
}

void RawDecoder::setMetaData(const CameraMetaData* meta, const string& make,
                             const string& model, const string& mode,
                             int iso_speed) {
  mRaw->metadata.isoSpeed = iso_speed;
  const Camera* cam = meta->getCamera(make, model, mode);
  if (!cam) {
    askForSamples(meta, make, model, mode);

    if (failOnUnknown)
      ThrowRDE("Camera '%s' '%s', mode '%s' not supported, and not allowed to guess. Sorry.", make.c_str(), model.c_str(), mode.c_str());

    return;
  }

  mRaw->cfa = cam->cfa;
  mRaw->metadata.canonical_make = cam->canonical_make;
  mRaw->metadata.canonical_model = cam->canonical_model;
  mRaw->metadata.canonical_alias = cam->canonical_alias;
  mRaw->metadata.canonical_id = cam->canonical_id;
  mRaw->metadata.make = make;
  mRaw->metadata.model = model;
  mRaw->metadata.mode = mode;

  if (applyCrop) {
    iPoint2D new_size = cam->cropSize;

    // If crop size is negative, use relative cropping
    if (new_size.x <= 0)
      new_size.x = mRaw->dim.x - cam->cropPos.x + new_size.x;

    if (new_size.y <= 0)
      new_size.y = mRaw->dim.y - cam->cropPos.y + new_size.y;

    mRaw->subFrame(iRectangle2D(cam->cropPos, new_size));
  }

  const CameraSensorInfo *sensor = cam->getSensorInfo(iso_speed);
  mRaw->blackLevel = sensor->mBlackLevel;
  mRaw->whitePoint = sensor->mWhiteLevel;
  mRaw->blackAreas = cam->blackAreas;
  if (mRaw->blackAreas.empty() && !sensor->mBlackLevelSeparate.empty()) {
    auto cfaArea = mRaw->cfa.getSize().area();
    if (mRaw->isCFA && cfaArea <= sensor->mBlackLevelSeparate.size()) {
      for (uint32 i = 0; i < cfaArea; i++) {
        mRaw->blackLevelSeparate[i] = sensor->mBlackLevelSeparate[i];
      }
    } else if (!mRaw->isCFA && mRaw->getCpp() <= sensor->mBlackLevelSeparate.size()) {
      for (uint32 i = 0; i < mRaw->getCpp(); i++) {
        mRaw->blackLevelSeparate[i] = sensor->mBlackLevelSeparate[i];
      }
    }
  }

  // Allow overriding individual blacklevels. Values are in CFA order
  // (the same order as the in the CFA tag)
  // A hint could be:
  // <Hint name="override_cfa_black" value="10,20,30,20"/>
  string cfa_black = hints.get("override_cfa_black", string());
  if (!cfa_black.empty()) {
    vector<string> v = splitString(cfa_black, ',');
    if (v.size() != 4) {
      mRaw->setError("Expected 4 values '10,20,30,20' as values for override_cfa_black hint.");
    } else {
      for (int i = 0; i < 4; i++) {
        mRaw->blackLevelSeparate[i] = stoi(v[i]);
      }
    }
  }
}

rawspeed::RawImage RawDecoder::decodeRaw() {
  try {
    RawImage raw = decodeRawInternal();
    raw->checkMemIsInitialized();

    raw->metadata.pixelAspectRatio =
        hints.get("pixel_aspect_ratio", raw->metadata.pixelAspectRatio);
    if (interpolateBadPixels) {
      raw->fixBadPixels();
      raw->checkMemIsInitialized();
    }

    return raw;
  } catch (TiffParserException &e) {
    ThrowRDE("%s", e.what());
  } catch (FileIOException &e) {
    ThrowRDE("%s", e.what());
  } catch (IOException &e) {
    ThrowRDE("%s", e.what());
  }
}

void RawDecoder::decodeMetaData(const CameraMetaData* meta) {
  try {
    decodeMetaDataInternal(meta);
  } catch (TiffParserException &e) {
    ThrowRDE("%s", e.what());
  } catch (FileIOException &e) {
    ThrowRDE("%s", e.what());
  } catch (IOException &e) {
    ThrowRDE("%s", e.what());
  }
}

void RawDecoder::checkSupport(const CameraMetaData* meta) {
  try {
    checkSupportInternal(meta);
  } catch (TiffParserException &e) {
    ThrowRDE("%s", e.what());
  } catch (FileIOException &e) {
    ThrowRDE("%s", e.what());
  } catch (IOException &e) {
    ThrowRDE("%s", e.what());
  }
}

} // namespace rawspeed
