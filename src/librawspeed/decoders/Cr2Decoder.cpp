/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2015-2017 Roman Lebedev
    Copyright (C) 2017 Axel Waggershauser

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

#include "decoders/Cr2Decoder.h"
#include "MemorySanitizer.h"
#include "adt/Array1DRef.h"
#include "adt/Array2DRef.h"
#include "adt/Bit.h"
#include "adt/Casts.h"
#include "adt/Optional.h"
#include "adt/Point.h"
#include "common/RawImage.h"
#include "decoders/RawDecoderException.h"
#include "decompressors/Cr2Decompressor.h"
#include "decompressors/Cr2LJpegDecoder.h"
#include "interpolators/Cr2sRawInterpolator.h"
#include "io/Buffer.h"
#include "io/ByteStream.h"
#include "io/Endianness.h"
#include "metadata/Camera.h"
#include "metadata/ColorFilterArray.h"
#include "parsers/TiffParserException.h"
#include "tiff/TiffEntry.h"
#include "tiff/TiffIFD.h"
#include "tiff/TiffTag.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rawspeed {
class CameraMetaData;

bool Cr2Decoder::isAppropriateDecoder(const TiffRootIFD* rootIFD,
                                      [[maybe_unused]] Buffer file) {
  const auto id = rootIFD->getID();
  const std::string& make = id.make;
  const std::string& model = id.model;

  // FIXME: magic

  return make == "Canon" ||
         (make == "Kodak" && (model == "DCS520C" || model == "DCS560C"));
}

RawImage Cr2Decoder::decodeOldFormat() {
  uint32_t offset = 0;
  if (mRootIFD->getEntryRecursive(TiffTag::CANON_RAW_DATA_OFFSET)) {
    offset =
        mRootIFD->getEntryRecursive(TiffTag::CANON_RAW_DATA_OFFSET)->getU32();
  } else {
    // D2000 is oh so special...
    const auto* ifd = mRootIFD->getIFDWithTag(TiffTag::CFAPATTERN);
    if (!ifd->hasEntry(TiffTag::STRIPOFFSETS))
      ThrowRDE("Couldn't find offset");

    offset = ifd->getEntry(TiffTag::STRIPOFFSETS)->getU32();
  }

  ByteStream b(DataBuffer(mFile.getSubView(offset), Endianness::big));
  b.skipBytes(41);
  int height = b.getU16();
  int width = b.getU16();

  // some old models (1D/1DS/D2000C) encode two lines as one
  // see: FIX_CANON_HALF_HEIGHT_DOUBLE_WIDTH
  if (width > 2 * height) {
    height *= 2;
    width /= 2;
  }
  width *= 2; // components

  mRaw->dim = {width, height};

  const ByteStream bs(DataBuffer(mFile.getSubView(offset), Endianness::little));

  Cr2LJpegDecoder l(bs, mRaw);
  mRaw->createData();

  Cr2SliceWidths slicing(/*numSlices=*/1, /*sliceWidth=don't care*/ 0,
                         /*lastSliceWidth=*/implicit_cast<uint16_t>(width));
  l.decode(slicing);
  ljpegSamplePrecision = l.getSamplePrecision();

  // deal with D2000 GrayResponseCurve
  if (const TiffEntry* curve =
          mRootIFD->getEntryRecursive(static_cast<TiffTag>(0x123));
      curve && curve->type == TiffDataType::SHORT && curve->count == 4096) {
    auto table = curve->getU16Array(curve->count);
    RawImageCurveGuard curveHandler(&mRaw, table, uncorrectedRawValues);

    // Apply table
    if (!uncorrectedRawValues)
      mRaw->sixteenBitLookup();
  }

  return mRaw;
}

// for technical details about Cr2 mRAW/sRAW, see http://lclevy.free.fr/cr2/

RawImage Cr2Decoder::decodeNewFormat() {
  const TiffEntry* sensorInfoE =
      mRootIFD->getEntryRecursive(TiffTag::CANON_SENSOR_INFO);
  if (!sensorInfoE)
    ThrowTPE("failed to get SensorInfo from MakerNote");

  assert(sensorInfoE != nullptr);

  if (isSubSampled() != (getSubSampling() != iPoint2D{1, 1}))
    ThrowTPE("Subsampling sanity check failed");

  mRaw->dim = {sensorInfoE->getU16(1), sensorInfoE->getU16(2)};
  mRaw->setCpp(1);
  mRaw->isCFA = !isSubSampled();

  if (isSubSampled()) {
    iPoint2D& subSampling = mRaw->metadata.subsampling;
    subSampling = getSubSampling();
    if (subSampling.x <= 1 && subSampling.y <= 1)
      ThrowRDE("RAW is expected to be subsampled, but it's not");

    if (mRaw->dim.x % subSampling.x != 0)
      ThrowRDE("Raw width is not a multiple of horizontal subsampling factor");
    mRaw->dim.x /= subSampling.x;

    if (mRaw->dim.y % subSampling.y != 0)
      ThrowRDE("Raw height is not a multiple of vertical subsampling factor");
    mRaw->dim.y /= subSampling.y;

    mRaw->dim.x *= 2 + subSampling.x * subSampling.y;
  }

  const TiffIFD* raw = mRootIFD->getSubIFDs()[3].get();

  Cr2SliceWidths slicing;
  // there are four cases:
  // * there is a tag with three components,
  //   $ last two components are non-zero: all fine then.
  //   $ first two components are zero, last component is non-zero
  //     we let Cr2LJpegDecoder guess it (it'll throw if fails)
  //   $ else the image is considered corrupt.
  // * there is a tag with not three components, the image is considered
  // corrupt. $ there is no tag, we let Cr2LJpegDecoder guess it (it'll throw if
  // fails)
  if (const TiffEntry* cr2SliceEntry =
          raw->getEntryRecursive(TiffTag::CANONCR2SLICE);
      cr2SliceEntry) {
    if (cr2SliceEntry->count != 3) {
      ThrowRDE("Found RawImageSegmentation tag with %d elements, should be 3.",
               cr2SliceEntry->count);
    }

    if (cr2SliceEntry->getU16(1) != 0 && cr2SliceEntry->getU16(2) != 0) {
      // first component can be either zero or non-zero, don't care
      slicing = Cr2SliceWidths(/*numSlices=*/1 + cr2SliceEntry->getU16(0),
                               /*sliceWidth=*/cr2SliceEntry->getU16(1),
                               /*lastSliceWidth=*/cr2SliceEntry->getU16(2));
    } else if (cr2SliceEntry->getU16(0) == 0 && cr2SliceEntry->getU16(1) == 0 &&
               cr2SliceEntry->getU16(2) != 0) {
      // PowerShot G16, PowerShot S120, let Cr2LJpegDecoder guess.
    } else {
      ThrowRDE("Strange RawImageSegmentation tag: (%d, %d, %d), image corrupt.",
               cr2SliceEntry->getU16(0), cr2SliceEntry->getU16(1),
               cr2SliceEntry->getU16(2));
    }
  } // EOS 20D, EOS-1D Mark II, let Cr2LJpegDecoder guess.

  const uint32_t offset = raw->getEntry(TiffTag::STRIPOFFSETS)->getU32();
  const uint32_t count = raw->getEntry(TiffTag::STRIPBYTECOUNTS)->getU32();

  const ByteStream bs(
      DataBuffer(mFile.getSubView(offset, count), Endianness::little));

  Cr2LJpegDecoder d(bs, mRaw);
  mRaw->createData();
  d.decode(slicing);
  ljpegSamplePrecision = d.getSamplePrecision();

  assert(getSubSampling() == mRaw->metadata.subsampling);

  if (mRaw->metadata.subsampling.x > 1 || mRaw->metadata.subsampling.y > 1)
    sRawInterpolate();

  return mRaw;
}

RawImage Cr2Decoder::decodeRawInternal() {
  if (mRootIFD->getSubIFDs().size() < 4)
    return decodeOldFormat();
  else // NOLINT(readability-else-after-return): ok, here it make sense
    return decodeNewFormat();
}

void Cr2Decoder::checkSupportInternal(const CameraMetaData* meta) {
  auto id = mRootIFD->getID();
  // Check for sRaw mode
  if (isSubSampled()) {
    checkCameraSupported(meta, id, "sRaw1");
    return;
  }

  checkCameraSupported(meta, id, "");
}

namespace {

enum class ColorDataFormat : uint8_t {
  ColorData1,
  ColorData2,
  ColorData3,
  ColorData4,
  ColorData5,
  ColorData6,
  ColorData7,
  ColorData8,
};

[[nodiscard]] Optional<std::pair<ColorDataFormat, Optional<int>>>
deduceColorDataFormat(const TiffEntry* ccd) {
  // The original ColorData, detect by it's fixed size.
  if (ccd->count == 582)
    return {{ColorDataFormat::ColorData1, {}}};
  // Second incarnation of ColorData, still size-only detection.
  if (ccd->count == 653)
    return {{ColorDataFormat::ColorData2, {}}};
  // From now onwards, Canon has finally added a `version` field, use it.
  switch (int colorDataVersion = static_cast<int16_t>(ccd->getU16(0));
          colorDataVersion) {
  case 1:
    return {{ColorDataFormat::ColorData3, colorDataVersion}};
  case 2:
  case 3:
  case 4:
  case 5:
  case 6:
  case 7:
  case 9:
    return {{ColorDataFormat::ColorData4, colorDataVersion}};
  case -4:
  case -3:
    return {{ColorDataFormat::ColorData5, colorDataVersion}};
  case 10: {
    ColorDataFormat f = [count = ccd->count]() {
      switch (count) {
      case 1273:
      case 1275:
        return ColorDataFormat::ColorData6;
      default:
        return ColorDataFormat::ColorData7;
      }
    }();
    return {{f, colorDataVersion}};
  }
  case 11:
    return {{ColorDataFormat::ColorData7, colorDataVersion}};
  case 12:
  case 13:
  case 14:
  case 15:
    return {{ColorDataFormat::ColorData8, colorDataVersion}};
  default:
    break;
  }
  return std::nullopt;
}

[[nodiscard]] int getWhiteBalanceOffsetInColorData(ColorDataFormat f) {
  switch (f) {
    using enum ColorDataFormat;
  case ColorData1:
    return 50;
  case ColorData2:
    return 68;
  case ColorData3:
  case ColorData4:
  case ColorData6:
  case ColorData7:
  case ColorData8:
    return 126;
  case ColorData5:
    return 142;
  }
  __builtin_unreachable();
}

[[nodiscard]] Optional<std::pair<int, int>>
getBlackAndWhiteLevelOffsetsInColorData(ColorDataFormat f,
                                        Optional<int> colorDataVersion) {
  switch (f) {
    using enum ColorDataFormat;
  case ColorData1:
  case ColorData2:
  case ColorData3:
    // These seemingly did not contain `SpecularWhiteLevel` yet.
    return std::nullopt;
  case ColorData4:
    switch (*colorDataVersion) {
    case 2:
    case 3:
      return std::nullopt; // Still no `SpecularWhiteLevel`.
    case 4:
    case 5:
      return {{692, 697}};
    case 6:
    case 7:
      return {{715, 720}};
    case 9:
      return {{719, 724}};
    default:
      __builtin_unreachable();
    }
  case ColorData5:
    switch (*colorDataVersion) {
    case -4:
      return {{333, 1386}};
    case -3:
      return {{264, 662}};
    default:
      __builtin_unreachable();
    }
  case ColorData6:
    switch (*colorDataVersion) {
    case 10:
      return {{479, 484}};
    default:
      __builtin_unreachable();
    }
  case ColorData7:
    switch (*colorDataVersion) {
    case 10:
      return {{504, 509}};
    case 11:
      return {{728, 733}};
    default:
      __builtin_unreachable();
    }
  case ColorData8:
    switch (*colorDataVersion) {
    case 12:
    case 13:
    case 15:
      return {{778, 783}};
    case 14:
      return {{556, 561}};
    default:
      __builtin_unreachable();
    }
  }
  __builtin_unreachable();
}

[[nodiscard]] bool shouldRescaleBlackLevels(ColorDataFormat f,
                                            Optional<int> colorDataVersion) {
  return f != ColorDataFormat::ColorData5 || colorDataVersion != -3;
}

} // namespace

bool Cr2Decoder::decodeCanonColorData() const {
  const TiffEntry* wb = mRootIFD->getEntryRecursive(TiffTag::CANONCOLORDATA);
  if (!wb)
    return false;

  auto dsc = deduceColorDataFormat(wb);
  if (!dsc)
    return false;

  auto [f, ver] = *dsc;

  int offset = getWhiteBalanceOffsetInColorData(f);

  offset /= 2;
  mRaw->metadata.wbCoeffs[0] = static_cast<float>(wb->getU16(offset + 0));
  mRaw->metadata.wbCoeffs[1] = static_cast<float>(wb->getU16(offset + 1));
  mRaw->metadata.wbCoeffs[2] = static_cast<float>(wb->getU16(offset + 3));

  auto levelOffsets = getBlackAndWhiteLevelOffsetsInColorData(f, ver);
  if (!levelOffsets)
    return false;

  mRaw->whitePoint = wb->getU16(levelOffsets->second);

  mRaw->blackLevelSeparate =
      Array2DRef(mRaw->blackLevelSeparateStorage.data(), 2, 2);
  auto blackLevelSeparate1D = *mRaw->blackLevelSeparate->getAsArray1DRef();
  for (int c = 0; c != 4; ++c)
    blackLevelSeparate1D(c) = wb->getU16(c + levelOffsets->first);

  // In Canon MakerNotes, the levels are always unscaled, and are 14-bit,
  // and so if the LJpeg precision was lower, we need to adjust.
  constexpr int makernotesPrecision = 14;
  if (makernotesPrecision > ljpegSamplePrecision) {
    int bitDepthDiff = makernotesPrecision - ljpegSamplePrecision;
    assert(bitDepthDiff >= 1 && bitDepthDiff <= 12);
    if (shouldRescaleBlackLevels(f, ver)) {
      for (int c = 0; c != 4; ++c)
        blackLevelSeparate1D(c) >>= bitDepthDiff;
    }
    mRaw->whitePoint = *mRaw->whitePoint >> bitDepthDiff;
  }

  return true;
}

void Cr2Decoder::parseWhiteBalance() const {
  // Default white point is LJpeg sample precision.
  mRaw->whitePoint = (1U << ljpegSamplePrecision) - 1;

  if (decodeCanonColorData())
    return;

  if (mRootIFD->hasEntryRecursive(TiffTag::CANONSHOTINFO) &&
      mRootIFD->hasEntryRecursive(TiffTag::CANONPOWERSHOTG9WB)) {
    const TiffEntry* shot_info =
        mRootIFD->getEntryRecursive(TiffTag::CANONSHOTINFO);
    const TiffEntry* g9_wb =
        mRootIFD->getEntryRecursive(TiffTag::CANONPOWERSHOTG9WB);

    uint16_t wb_index = shot_info->getU16(7);
    int wb_offset = (wb_index < 18)
                        ? std::string_view("012347800000005896")[wb_index] - '0'
                        : 0;
    wb_offset = wb_offset * 8 + 2;

    mRaw->metadata.wbCoeffs[0] =
        static_cast<float>(g9_wb->getU32(wb_offset + 1));
    mRaw->metadata.wbCoeffs[1] =
        (static_cast<float>(g9_wb->getU32(wb_offset + 0)) +
         static_cast<float>(g9_wb->getU32(wb_offset + 3))) /
        2.0F;
    mRaw->metadata.wbCoeffs[2] =
        static_cast<float>(g9_wb->getU32(wb_offset + 2));
  } else if (mRootIFD->hasEntryRecursive(static_cast<TiffTag>(0xa4))) {
    // WB for the old 1D and 1DS
    const TiffEntry* wb =
        mRootIFD->getEntryRecursive(static_cast<TiffTag>(0xa4));
    if (wb->count >= 3) {
      mRaw->metadata.wbCoeffs[0] = wb->getFloat(0);
      mRaw->metadata.wbCoeffs[1] = wb->getFloat(1);
      mRaw->metadata.wbCoeffs[2] = wb->getFloat(2);
    }
  }
}

void Cr2Decoder::decodeMetaDataInternal(const CameraMetaData* meta) {
  int iso = 0;
  mRaw->cfa.setCFA(iPoint2D(2, 2), CFAColor::RED, CFAColor::GREEN,
                   CFAColor::GREEN, CFAColor::BLUE);

  std::string mode;

  if (mRaw->metadata.subsampling.y == 2 && mRaw->metadata.subsampling.x == 2)
    mode = "sRaw1";

  if (mRaw->metadata.subsampling.y == 1 && mRaw->metadata.subsampling.x == 2)
    mode = "sRaw2";

  if (mRootIFD->hasEntryRecursive(TiffTag::ISOSPEEDRATINGS))
    iso = mRootIFD->getEntryRecursive(TiffTag::ISOSPEEDRATINGS)->getU32();
  if (65535 == iso) {
    // ISOSPEEDRATINGS is a SHORT EXIF value. For larger values, we have to look
    // at RECOMMENDEDEXPOSUREINDEX (maybe Canon specific).
    if (mRootIFD->hasEntryRecursive(TiffTag::RECOMMENDEDEXPOSUREINDEX))
      iso = mRootIFD->getEntryRecursive(TiffTag::RECOMMENDEDEXPOSUREINDEX)
                ->getU32();
  }

  // Fetch the white balance
  try {
    parseWhiteBalance();
  } catch (const RawspeedException& e) {
    mRaw->setError(e.what());
    // We caught an exception reading WB, just ignore it
  }
  setMetaData(meta, mode, iso);
  assert(mShiftUpScaleForExif == 0 || mShiftUpScaleForExif == 2);
  if (mShiftUpScaleForExif) {
    mRaw->blackLevel = 0;
    mRaw->blackLevelSeparate = std::nullopt;
  }
  if (mShiftUpScaleForExif != 0 && isPowerOfTwo(1 + *mRaw->whitePoint))
    mRaw->whitePoint = ((1 + *mRaw->whitePoint) << mShiftUpScaleForExif) - 1;
  else
    mRaw->whitePoint = *mRaw->whitePoint << mShiftUpScaleForExif;
}

bool Cr2Decoder::isSubSampled() const {
  if (mRootIFD->getSubIFDs().size() != 4)
    return false;
  const TiffEntry* typeE =
      mRootIFD->getSubIFDs()[3]->getEntryRecursive(TiffTag::CANON_SRAWTYPE);
  return typeE && typeE->getU32() == 4;
}

iPoint2D Cr2Decoder::getSubSampling() const {
  const TiffEntry* CCS =
      mRootIFD->getEntryRecursive(TiffTag::CANON_CAMERA_SETTINGS);
  if (!CCS)
    ThrowRDE("CanonCameraSettings entry not found.");

  if (CCS->type != TiffDataType::SHORT)
    ThrowRDE("Unexpected CanonCameraSettings entry type encountered ");

  if (CCS->count < 47)
    return {1, 1};

  switch (uint16_t qual = CCS->getU16(46)) {
  case 0:
    return {1, 1};
  case 1:
    return {2, 2};
  case 2:
    return {2, 1};
  default:
    ThrowRDE("Unexpected SRAWQuality value found: %u", qual);
  }
}

int Cr2Decoder::getHue() const {
  if (hints.contains("old_sraw_hue"))
    return (mRaw->metadata.subsampling.y * mRaw->metadata.subsampling.x);

  if (!mRootIFD->hasEntryRecursive(static_cast<TiffTag>(0x10))) {
    return 0;
  }
  if (uint32_t model_id =
          mRootIFD->getEntryRecursive(static_cast<TiffTag>(0x10))->getU32();
      model_id >= 0x80000281 || model_id == 0x80000218 ||
      (hints.contains("force_new_sraw_hue"))) {
    return ((mRaw->metadata.subsampling.y * mRaw->metadata.subsampling.x) -
            1) >>
           1;
  }

  return (mRaw->metadata.subsampling.y * mRaw->metadata.subsampling.x);
}

// Interpolate and convert sRaw data.
void Cr2Decoder::sRawInterpolate() {
  const TiffEntry* wb = mRootIFD->getEntryRecursive(TiffTag::CANONCOLORDATA);
  if (!wb)
    ThrowRDE("Unable to locate WB info.");

  // Offset to sRaw coefficients used to reconstruct uncorrected RGB data.
  uint32_t offset = 78;

  std::array<int, 3> sraw_coeffs;

  assert(wb != nullptr);
  sraw_coeffs[0] = wb->getU16(offset + 0);
  sraw_coeffs[1] = (wb->getU16(offset + 1) + wb->getU16(offset + 2) + 1) >> 1;
  sraw_coeffs[2] = wb->getU16(offset + 3);

  if (hints.contains("invert_sraw_wb")) {
    sraw_coeffs[0] = static_cast<int>(
        1024.0F / (static_cast<float>(sraw_coeffs[0]) / 1024.0F));
    sraw_coeffs[2] = static_cast<int>(
        1024.0F / (static_cast<float>(sraw_coeffs[2]) / 1024.0F));
  }

  MSan::CheckMemIsInitialized(mRaw->getByteDataAsUncroppedArray2DRef());
  RawImage subsampledRaw = mRaw;
  int hue = getHue();

  iPoint2D interpolatedDims = {
      subsampledRaw->metadata.subsampling.x *
          (subsampledRaw->dim.x /
           (2 + subsampledRaw->metadata.subsampling.x *
                    subsampledRaw->metadata.subsampling.y)),
      subsampledRaw->metadata.subsampling.y * subsampledRaw->dim.y};

  mRaw = RawImage::create(interpolatedDims, RawImageType::UINT16, 3);
  mRaw->metadata.subsampling = subsampledRaw->metadata.subsampling;
  mRaw->isCFA = false;

  Cr2sRawInterpolator i(mRaw, subsampledRaw->getU16DataAsUncroppedArray2DRef(),
                        sraw_coeffs, hue);

  /* Determine sRaw coefficients */
  bool isOldSraw = hints.contains("sraw_40d");
  bool isNewSraw = hints.contains("sraw_new");

  int version;
  if (isOldSraw)
    version = 0;
  else {
    if (isNewSraw) {
      version = 2;
    } else {
      version = 1;
    }
  }

  i.interpolate(version);

  mShiftUpScaleForExif = 2;
}

} // namespace rawspeed
