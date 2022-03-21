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
#include "common/Common.h"                          // for roundUpDivision
#include "common/Point.h"                           // for iPoint2D, iRecta...
#include "decoders/RawDecoderException.h"           // for ThrowRDE
#include "decompressors/UncompressedDecompressor.h" // for UncompressedDeco...
#include "io/Buffer.h"                              // for Buffer, DataBuffer
#include "io/ByteStream.h"                          // for ByteStream
#include "io/Endianness.h"                          // for Endianness, Endi...
#include "io/FileIOException.h"                     // for FileIOException
#include "io/IOException.h"                         // for IOException
#include "metadata/BlackArea.h"                     // for BlackArea
#include "metadata/Camera.h"                        // for Camera, Hints
#include "metadata/CameraMetaData.h"                // for CameraMetaData
#include "metadata/CameraSensorInfo.h"              // for CameraSensorInfo
#include "metadata/ColorFilterArray.h"              // for ColorFilterArray
#include "parsers/TiffParserException.h"            // for TiffParserException
#include "tiff/TiffEntry.h"                         // for TiffEntry
#include "tiff/TiffIFD.h"                           // for TiffIFD
#include "tiff/TiffTag.h"                           // for BITSPERSAMPLE
#include <algorithm>                                // for max
#include <array>                                    // for array
#include <cassert>                                  // for assert
#include <string>                                   // for string, basic_st...
#include <vector>                                   // for vector

using std::vector;

namespace rawspeed {

RawDecoder::RawDecoder(const Buffer& file)
    : failOnUnknown(false), interpolateBadPixels(true),
      applyStage1DngOpcodes(true), applyCrop(true), uncorrectedRawValues(false),
      fujiRotate(true), mFile(file) {mRaw.appendFrame(new RawImageDataU16());}

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

  mRaw.get(0)->dim = iPoint2D(width, height);

  if (counts->count != offsets->count) {
    ThrowRDE("Byte count number does not match strip size: "
             "count:%u, stips:%u ",
             counts->count, offsets->count);
  }

  if (yPerSlice == 0 || yPerSlice > static_cast<uint32_t>(mRaw.get(0)->dim.y) ||
      roundUpDivision(mRaw.get(0)->dim.y, yPerSlice) != counts->count) {
    ThrowRDE("Invalid y per slice %u or strip count %u (height = %u)",
             yPerSlice, counts->count, mRaw.get(0)->dim.y);
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

  mRaw.get(0)->createData();

  // Default white level is (2 ** BitsPerSample) - 1
  mRaw.get(0)->whitePoint = (1UL << bitPerPixel) - 1UL;

  offY = 0;
  for (const RawSlice& slice : slices) {
    UncompressedDecompressor u(
        ByteStream(DataBuffer(mFile.getSubView(slice.offset, slice.count),
                              Endianness::little)),
        mRaw.get(0).get());
    iPoint2D size(width, slice.h);
    iPoint2D pos(0, offY);
    bitPerPixel = (static_cast<uint64_t>(slice.count) * 8U) / (slice.h * width);
    const auto inputPitch = width * bitPerPixel / 8;
    if (!inputPitch)
      ThrowRDE("Bad input pitch. Can not decode anything.");

    u.readUncompressedRaw(size, pos, inputPitch, bitPerPixel, order);

    offY += slice.h;
  }
}

void RawDecoder::askForSamples([[maybe_unused]] const CameraMetaData* meta,
                               const std::string& make,
                               const std::string& model,
                               const std::string& mode) {
  if ("dng" == mode)
    return;

  writeLog(DEBUG_PRIO::WARNING,
           "Unable to find camera in database: '%s' '%s' "
           "'%s'\nPlease consider providing samples on "
           "<https://raw.pixls.us/>, thanks!",
           make.c_str(), model.c_str(), mode.c_str());
}

bool RawDecoder::checkCameraSupported(const CameraMetaData* meta,
                                      const std::string& make,
                                      const std::string& model,
                                      const std::string& mode) {
  mRaw.get(0)->metadata.make = make;
  mRaw.get(0)->metadata.model = model;
  const Camera* cam = meta->getCamera(make, model, mode);
  if (!cam) {
    askForSamples(meta, make, model, mode);

    if (failOnUnknown)
      ThrowRDE("Camera '%s' '%s', mode '%s' not supported, and not allowed to guess. Sorry.", make.c_str(), model.c_str(), mode.c_str());

    // Assume the camera can be decoded, but return false, so decoders can see that we are unsure.
    return false;
  }

  switch (cam->supportStatus) {
  case Camera::SupportStatus::Supported:
    break; // Yay us!
  case Camera::SupportStatus::Unsupported:
    ThrowRDE("Camera not supported (explicit). Sorry.");
  case Camera::SupportStatus::NoSamples:
    noSamples = true;
    writeLog(DEBUG_PRIO::WARNING,
             "Camera support status is unknown: '%s' '%s' '%s'\n"
             "Please consider providing samples on <https://raw.pixls.us/> "
             "if you wish for the support to not be discontinued, thanks!",
             make.c_str(), model.c_str(), mode.c_str());
    break; // WYSIWYG.
  }

  if (cam->decoderVersion > getDecoderVersion())
    ThrowRDE("Camera not supported in this version. Update RawSpeed for support.");

  hints = cam->hints;
  return true;
}

void RawDecoder::setMetaData(const CameraMetaData* meta,
                             const std::string& make, const std::string& model,
                             const std::string& mode, int iso_speed) {
  mRaw.get(0)->metadata.isoSpeed = iso_speed;
  const Camera* cam = meta->getCamera(make, model, mode);
  if (!cam) {
    askForSamples(meta, make, model, mode);

    if (failOnUnknown)
      ThrowRDE("Camera '%s' '%s', mode '%s' not supported, and not allowed to guess. Sorry.", make.c_str(), model.c_str(), mode.c_str());

    return;
  }

  // Only override CFA with the data from cameras.xml if it actually contained
  // the CFA.
  if (cam->cfa.getSize().area() > 0)
    mRaw.get(0)->cfa = cam->cfa;

  if (!cam->color_matrix.empty())
    mRaw.get(0)->metadata.colorMatrix = cam->color_matrix;

  mRaw.get(0)->metadata.canonical_make = cam->canonical_make;
  mRaw.get(0)->metadata.canonical_model = cam->canonical_model;
  mRaw.get(0)->metadata.canonical_alias = cam->canonical_alias;
  mRaw.get(0)->metadata.canonical_id = cam->canonical_id;
  mRaw.get(0)->metadata.make = make;
  mRaw.get(0)->metadata.model = model;
  mRaw.get(0)->metadata.mode = mode;

  if (applyCrop) {
    iPoint2D new_size = cam->cropSize;

    // If crop size is negative, use relative cropping
    if (new_size.x <= 0)
      new_size.x = mRaw.get(0)->dim.x - cam->cropPos.x + new_size.x;

    if (new_size.y <= 0)
      new_size.y = mRaw.get(0)->dim.y - cam->cropPos.y + new_size.y;

    mRaw.get(0)->subFrame(iRectangle2D(cam->cropPos, new_size));
  }

  const CameraSensorInfo *sensor = cam->getSensorInfo(iso_speed);
  mRaw.get(0)->blackLevel = sensor->mBlackLevel;
  mRaw.get(0)->whitePoint = sensor->mWhiteLevel;
  mRaw.get(0)->blackAreas = cam->blackAreas;
  if (mRaw.get(0)->blackAreas.empty() && !sensor->mBlackLevelSeparate.empty()) {
    auto cfaArea = mRaw.get(0)->cfa.getSize().area();
    if (mRaw.get(0)->isCFA && cfaArea <= sensor->mBlackLevelSeparate.size()) {
      for (auto i = 0UL; i < cfaArea; i++) {
        mRaw.get(0)->blackLevelSeparate[i] = sensor->mBlackLevelSeparate[i];
      }
    } else if (!mRaw.get(0)->isCFA && mRaw.get(0)->getCpp() <= sensor->mBlackLevelSeparate.size()) {
      for (uint32_t i = 0; i < mRaw.get(0)->getCpp(); i++) {
        mRaw.get(0)->blackLevelSeparate[i] = sensor->mBlackLevelSeparate[i];
      }
    }
  }

  // Allow overriding individual blacklevels. Values are in CFA order
  // (the same order as the in the CFA tag)
  // A hint could be:
  // <Hint name="override_cfa_black" value="10,20,30,20"/>
  std::string cfa_black = hints.get("override_cfa_black", std::string());
  if (!cfa_black.empty()) {
    vector<std::string> v = splitString(cfa_black, ',');
    if (v.size() != 4) {
      mRaw.get(0)->setError("Expected 4 values '10,20,30,20' as values for "
                     "override_cfa_black hint.");
    } else {
      for (int i = 0; i < 4; i++) {
        mRaw.get(0)->blackLevelSeparate[i] = stoi(v[i]);
      }
    }
  }
}

void RawDecoder::decodeRaw() {
  try {
    decodeRawInternal();
    mRaw.get(0)->checkMemIsInitialized();

    mRaw.get(0)->metadata.pixelAspectRatio =
        hints.get("pixel_aspect_ratio", mRaw.get(0)->metadata.pixelAspectRatio);
    if (interpolateBadPixels) {
      mRaw.get(0)->fixBadPixels();
      mRaw.get(0)->checkMemIsInitialized();
    }


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
