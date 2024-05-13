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

#include "decoders/RawDecoder.h"
#include "MemorySanitizer.h"
#include "adt/Array1DRef.h"
#include "adt/Array2DRef.h"
#include "adt/Casts.h"
#include "adt/Point.h"
#include "bitstreams/BitStreams.h"
#include "common/Common.h"
#include "common/RawImage.h"
#include "decompressors/UncompressedDecompressor.h"
#include "io/Buffer.h"
#include "io/ByteStream.h"
#include "io/Endianness.h"
#include "io/FileIOException.h"
#include "io/IOException.h"
#include "metadata/Camera.h"
#include "metadata/CameraMetaData.h"
#include "metadata/CameraSensorInfo.h"
#include "metadata/ColorFilterArray.h"
#include "parsers/TiffParserException.h"
#include "tiff/TiffEntry.h"
#include "tiff/TiffIFD.h"
#include "tiff/TiffTag.h"
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

using std::vector;

namespace rawspeed {

RawDecoder::RawDecoder(Buffer file) : mFile(file) {}

void RawDecoder::decodeUncompressed(const TiffIFD* rawIFD,
                                    BitOrder order) const {
  const TiffEntry* offsets = rawIFD->getEntry(TiffTag::STRIPOFFSETS);
  const TiffEntry* counts = rawIFD->getEntry(TiffTag::STRIPBYTECOUNTS);
  uint32_t yPerSlice = rawIFD->getEntry(TiffTag::ROWSPERSTRIP)->getU32();
  uint32_t width = rawIFD->getEntry(TiffTag::IMAGEWIDTH)->getU32();
  uint32_t height = rawIFD->getEntry(TiffTag::IMAGELENGTH)->getU32();
  uint32_t bitPerPixel = rawIFD->getEntry(TiffTag::BITSPERSAMPLE)->getU32();

  if (width == 0 || height == 0 || width > 5632 || height > 3720)
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", width, height);

  mRaw->dim = iPoint2D(width, height);

  if (counts->count != offsets->count) {
    ThrowRDE("Byte count number does not match strip size: "
             "count:%u, stips:%u ",
             counts->count, offsets->count);
  }

  if (yPerSlice == 0 || yPerSlice > static_cast<uint32_t>(mRaw->dim.y) ||
      roundUpDivisionSafe(mRaw->dim.y, yPerSlice) != counts->count) {
    ThrowRDE("Invalid y per slice %u or strip count %u (height = %u)",
             yPerSlice, counts->count, mRaw->dim.y);
  }

  switch (bitPerPixel) {
  case 12:
  case 14:
    break;
  default:
    ThrowRDE("Unexpected bits per pixel: %u.", bitPerPixel);
  }

  vector<RawSlice> slices;
  slices.reserve(counts->count);
  uint32_t offY = 0;

  for (uint32_t s = 0; s < counts->count; s++) {
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

    if (!mFile.isValid(slice.offset, slice.count))
      ThrowRDE("Slice offset/count invalid");

    slices.push_back(slice);
  }

  if (slices.empty())
    ThrowRDE("No valid slices found. File probably truncated.");

  assert(height <= offY);
  assert(slices.size() == counts->count);

  mRaw->createData();

  // Default white level is (2 ** BitsPerSample) - 1
  mRaw->whitePoint = implicit_cast<int>((1UL << bitPerPixel) - 1UL);

  offY = 0;
  for (const RawSlice& slice : slices) {
    iPoint2D size(width, slice.h);
    iPoint2D pos(0, offY);
    bitPerPixel = implicit_cast<uint32_t>(
        (static_cast<uint64_t>(slice.count) * 8U) / (slice.h * width));
    const auto inputPitch = width * bitPerPixel / 8;
    if (!inputPitch)
      ThrowRDE("Bad input pitch. Can not decode anything.");

    UncompressedDecompressor u(
        ByteStream(DataBuffer(mFile.getSubView(slice.offset, slice.count),
                              Endianness::little)),
        mRaw, iRectangle2D(pos, size), inputPitch, bitPerPixel, order);
    u.readUncompressedRaw();

    offY += slice.h;
  }
}

bool RawDecoder::handleCameraSupport(const CameraMetaData* meta,
                                     const std::string& make,
                                     const std::string& model,
                                     const std::string& mode) {
  Camera::SupportStatus supportStatus = Camera::SupportStatus::UnknownCamera;
  const Camera* cam = meta->getCamera(make, model, mode);
  if (cam)
    supportStatus = cam->supportStatus;

  // Sample beggary block.
  switch (supportStatus) {
    using enum Camera::SupportStatus;
  case UnknownCamera:
    if ("dng" != mode) {
      noSamples = true;
      writeLog(DEBUG_PRIO::WARNING,
               "Unable to find camera in database: '%s' '%s' '%s'\nPlease "
               "consider providing samples on <https://raw.pixls.us/>, thanks!",
               make.c_str(), model.c_str(), mode.c_str());
    }
    break;
  case UnknownNoSamples:
  case SupportedNoSamples:
    noSamples = true;
    writeLog(DEBUG_PRIO::WARNING,
             "Camera support status is unknown: '%s' '%s' '%s'\n"
             "Please consider providing samples on <https://raw.pixls.us/> "
             "if you wish for the support to not be discontinued, thanks!",
             make.c_str(), model.c_str(), mode.c_str());
    break; // WYSIWYG.
  case Supported:
  case Unknown:
  case Unsupported:
    break; // All these imply existence of a sample on RPU.
  }

  // Actual support handling.
  switch (supportStatus) {
    using enum Camera::SupportStatus;
  case Supported:
  case SupportedNoSamples:
    return true; // Explicitly supported.
  case Unsupported:
    ThrowRDE("Camera not supported (explicit). Sorry.");
  case UnknownCamera:
  case UnknownNoSamples:
  case Unknown:
    if (failOnUnknown) {
      ThrowRDE("Camera '%s' '%s', mode '%s' not supported, and not allowed to "
               "guess. Sorry.",
               make.c_str(), model.c_str(), mode.c_str());
    }
    return cam; // Might be implicitly supported.
  }

  return true;
}

bool RawDecoder::checkCameraSupported(const CameraMetaData* meta,
                                      const std::string& make,
                                      const std::string& model,
                                      const std::string& mode) {
  mRaw->metadata.make = make;
  mRaw->metadata.model = model;

  if (!handleCameraSupport(meta, make, model, mode))
    return false;

  const Camera* cam = meta->getCamera(make, model, mode);
  assert(cam);

  if (cam->decoderVersion > getDecoderVersion())
    ThrowRDE(
        "Camera not supported in this version. Update RawSpeed for support.");

  hints = cam->hints;
  return true;
}

void RawDecoder::setMetaData(const CameraMetaData* meta,
                             const std::string& make, const std::string& model,
                             const std::string& mode, int iso_speed) {
  mRaw->metadata.isoSpeed = iso_speed;

  if (!handleCameraSupport(meta, make, model, mode))
    return;

  const Camera* cam = meta->getCamera(make, model, mode);
  assert(cam);

  // Only final CFA with the data from cameras.xml if it actually contained
  // the CFA.
  if (cam->cfa.getSize().area() > 0)
    mRaw->cfa = cam->cfa;

  if (!cam->color_matrix.empty())
    mRaw->metadata.colorMatrix = cam->color_matrix;

  mRaw->metadata.canonical_make = cam->canonical_make;
  mRaw->metadata.canonical_model = cam->canonical_model;
  mRaw->metadata.canonical_alias = cam->canonical_alias;
  mRaw->metadata.canonical_id = cam->canonical_id;
  mRaw->metadata.make = make;
  mRaw->metadata.model = model;
  mRaw->metadata.mode = mode;

  if (applyCrop) {
    if (cam->cropAvailable) {
      iPoint2D new_size = cam->cropSize;

      // If crop size is negative, use relative cropping
      if (new_size.x <= 0)
        new_size.x = mRaw->dim.x - cam->cropPos.x + new_size.x;

      if (new_size.y <= 0)
        new_size.y = mRaw->dim.y - cam->cropPos.y + new_size.y;

      mRaw->subFrame(iRectangle2D(cam->cropPos, new_size));
    } else {
      mRaw->subFrame(getDefaultCrop());
    }
  }

  mRaw->blackAreas = cam->blackAreas;
  if (const CameraSensorInfo* sensor = cam->getSensorInfo(iso_speed)) {
    mRaw->blackLevel = sensor->mBlackLevel;
    mRaw->whitePoint = sensor->mWhiteLevel;
    if (mRaw->blackAreas.empty() && !sensor->mBlackLevelSeparate.empty()) {
      auto cfaArea = implicit_cast<int>(mRaw->cfa.getSize().area());
      if (mRaw->isCFA &&
          cfaArea <= implicit_cast<int>(sensor->mBlackLevelSeparate.size())) {
        mRaw->blackLevelSeparate =
            Array2DRef(mRaw->blackLevelSeparateStorage.data(), 2, 2);
        auto blackLevelSeparate1D =
            *mRaw->blackLevelSeparate->getAsArray1DRef();
        for (int i = 0; i < cfaArea; i++) {
          blackLevelSeparate1D(i) = sensor->mBlackLevelSeparate[i];
        }
      } else if (!mRaw->isCFA &&
                 mRaw->getCpp() <= sensor->mBlackLevelSeparate.size()) {
        mRaw->blackLevelSeparate =
            Array2DRef(mRaw->blackLevelSeparateStorage.data(), 2, 2);
        auto blackLevelSeparate1D =
            *mRaw->blackLevelSeparate->getAsArray1DRef();
        for (uint32_t i = 0; i < mRaw->getCpp(); i++) {
          blackLevelSeparate1D(i) = sensor->mBlackLevelSeparate[i];
        }
      }
    }
  }

  // Allow overriding individual blacklevels. Values are in CFA order
  // (the same order as the in the CFA tag)
  // A hint could be:
  // <Hint name="final_cfa_black" value="10,20,30,20"/>
  std::string cfa_black = hints.get("final_cfa_black", std::string());
  if (!cfa_black.empty()) {
    vector<std::string> v = splitString(cfa_black, ',');
    if (v.size() != 4) {
      mRaw->setError("Expected 4 values '10,20,30,20' as values for "
                     "final_cfa_black hint.");
    } else {
      auto blackLevelSeparate1D = *mRaw->blackLevelSeparate->getAsArray1DRef();
      for (int i = 0; i < 4; i++) {
        blackLevelSeparate1D(i) = stoi(v[i]);
      }
    }
  }
}

rawspeed::iRectangle2D RawDecoder::getDefaultCrop() {
  return {mRaw->dim.x, mRaw->dim.y};
}

rawspeed::RawImage RawDecoder::decodeRaw() {
  try {
    RawImage raw = decodeRawInternal();
    MSan::CheckMemIsInitialized(raw->getByteDataAsUncroppedArray2DRef());

    raw->metadata.pixelAspectRatio =
        hints.get("pixel_aspect_ratio", raw->metadata.pixelAspectRatio);
    if (interpolateBadPixels) {
      raw->fixBadPixels();
      MSan::CheckMemIsInitialized(raw->getByteDataAsUncroppedArray2DRef());
    }

    return raw;
  } catch (const TiffParserException& e) {
    ThrowRDE("%s", e.what());
  } catch (const FileIOException& e) {
    ThrowRDE("%s", e.what());
  } catch (const IOException& e) {
    ThrowRDE("%s", e.what());
  }
}

void RawDecoder::decodeMetaData(const CameraMetaData* meta) {
  try {
    decodeMetaDataInternal(meta);
  } catch (const TiffParserException& e) {
    ThrowRDE("%s", e.what());
  } catch (const FileIOException& e) {
    ThrowRDE("%s", e.what());
  } catch (const IOException& e) {
    ThrowRDE("%s", e.what());
  }
}

void RawDecoder::checkSupport(const CameraMetaData* meta) {
  try {
    checkSupportInternal(meta);
  } catch (const TiffParserException& e) {
    ThrowRDE("%s", e.what());
  } catch (const FileIOException& e) {
    ThrowRDE("%s", e.what());
  } catch (const IOException& e) {
    ThrowRDE("%s", e.what());
  }
}

} // namespace rawspeed
