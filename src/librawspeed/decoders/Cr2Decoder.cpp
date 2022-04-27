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
#include "common/Point.h"                      // for iPoint2D
#include "common/RawspeedException.h"          // for RawspeedException
#include "decoders/RawDecoderException.h"      // for ThrowRDE
#include "decompressors/Cr2Decompressor.h"     // for Cr2Decompressor, Cr2S...
#include "interpolators/Cr2sRawInterpolator.h" // for Cr2sRawInterpolator
#include "io/Buffer.h"                         // for Buffer, DataBuffer
#include "io/ByteStream.h"                     // for ByteStream
#include "io/Endianness.h"                     // for Endianness, Endiannes...
#include "metadata/Camera.h"                   // for Hints
#include "metadata/ColorFilterArray.h" // for CFAColor::GREEN, CFAColor::BLUE
#include "parsers/TiffParserException.h"       // for ThrowTPE
#include "tiff/TiffEntry.h" // for TiffEntry, TiffDataType::SHORT
#include "tiff/TiffTag.h"                      // for TiffTag, CANONCOLORDATA
#include <array>                               // for array
#include <cassert>                             // for assert
#include <cstdint>                             // for uint32_t, uint16_t
#include <memory>                              // for unique_ptr, allocator...
#include <string>                              // for operator==, string
#include <vector>                              // for vector
// IWYU pragma: no_include <ext/alloc_traits.h>

namespace rawspeed {

bool Cr2Decoder::isAppropriateDecoder(const TiffRootIFD* rootIFD,
                                      [[maybe_unused]] const Buffer& file) {
  const auto id = rootIFD->getID();
  const std::string& make = id.make;
  const std::string& model = id.model;

  // FIXME: magic

  return make == "Canon" ||
         (make == "Kodak" && (model == "DCS520C" || model == "DCS560C"));
}

void Cr2Decoder::decodeOldFormat() {
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

  Cr2Decompressor l(bs, mRaw);
  mRaw->createData();

  Cr2Slicing slicing(/*numSlices=*/1, /*sliceWidth=don't care*/ 0,
                     /*lastSliceWidth=*/width);
  l.decode(slicing);

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
}

// for technical details about Cr2 mRAW/sRAW, see http://lclevy.free.fr/cr2/

void Cr2Decoder::decodeNewFormat() {
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
    if (!(subSampling.x > 1 || subSampling.y > 1))
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

  Cr2Slicing slicing;
  // there are four cases:
  // * there is a tag with three components,
  //   $ last two components are non-zero: all fine then.
  //   $ first two components are zero, last component is non-zero
  //     we let Cr2Decompressor guess it (it'll throw if fails)
  //   $ else the image is considered corrupt.
  // * there is a tag with not three components, the image is considered
  // corrupt. $ there is no tag, we let Cr2Decompressor guess it (it'll throw if
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
      slicing = Cr2Slicing(/*numSlices=*/1 + cr2SliceEntry->getU16(0),
                           /*sliceWidth=*/cr2SliceEntry->getU16(1),
                           /*lastSliceWidth=*/cr2SliceEntry->getU16(2));
    } else if (cr2SliceEntry->getU16(0) == 0 && cr2SliceEntry->getU16(1) == 0 &&
               cr2SliceEntry->getU16(2) != 0) {
      // PowerShot G16, PowerShot S120, let Cr2Decompressor guess.
    } else {
      ThrowRDE("Strange RawImageSegmentation tag: (%d, %d, %d), image corrupt.",
               cr2SliceEntry->getU16(0), cr2SliceEntry->getU16(1),
               cr2SliceEntry->getU16(2));
    }
  } // EOS 20D, EOS-1D Mark II, let Cr2Decompressor guess.

  const uint32_t offset = raw->getEntry(TiffTag::STRIPOFFSETS)->getU32();
  const uint32_t count = raw->getEntry(TiffTag::STRIPBYTECOUNTS)->getU32();

  const ByteStream bs(
      DataBuffer(mFile.getSubView(offset, count), Endianness::little));

  Cr2Decompressor d(bs, mRaw);
  mRaw->createData();
  d.decode(slicing);

  assert(getSubSampling() == mRaw->metadata.subsampling);

  if (mRaw->metadata.subsampling.x > 1 || mRaw->metadata.subsampling.y > 1)
    sRawInterpolate();
}

void Cr2Decoder::decodeRawInternal() {
  if (mRootIFD->getSubIFDs().size() < 4)
    decodeOldFormat();
  else // NOLINT ok, here it make sense
    decodeNewFormat();
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
  if(65535 == iso) {
    // ISOSPEEDRATINGS is a SHORT EXIF value. For larger values, we have to look
    // at RECOMMENDEDEXPOSUREINDEX (maybe Canon specific).
    if (mRootIFD->hasEntryRecursive(TiffTag::RECOMMENDEDEXPOSUREINDEX))
      iso = mRootIFD->getEntryRecursive(TiffTag::RECOMMENDEDEXPOSUREINDEX)
                ->getU32();
  }

  // Fetch the white balance
  try{
    if (mRootIFD->hasEntryRecursive(TiffTag::CANONCOLORDATA)) {
      const TiffEntry* wb =
          mRootIFD->getEntryRecursive(TiffTag::CANONCOLORDATA);
      // this entry is a big table, and different cameras store used WB in
      // different parts, so find the offset, default is the most common one
      int offset = hints.get("wb_offset", 126);

      offset /= 2;
      mRaw->metadata.wbCoeffs[0] = static_cast<float>(wb->getU16(offset + 0));
      mRaw->metadata.wbCoeffs[1] = static_cast<float>(wb->getU16(offset + 1));
      mRaw->metadata.wbCoeffs[2] = static_cast<float>(wb->getU16(offset + 3));
    } else {
      if (mRootIFD->hasEntryRecursive(TiffTag::CANONSHOTINFO) &&
          mRootIFD->hasEntryRecursive(TiffTag::CANONPOWERSHOTG9WB)) {
        const TiffEntry* shot_info =
            mRootIFD->getEntryRecursive(TiffTag::CANONSHOTINFO);
        const TiffEntry* g9_wb =
            mRootIFD->getEntryRecursive(TiffTag::CANONPOWERSHOTG9WB);

        uint16_t wb_index = shot_info->getU16(7);
        int wb_offset = (wb_index < 18) ? "012347800000005896"[wb_index]-'0' : 0;
        wb_offset = wb_offset*8 + 2;

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
  } catch (const RawspeedException& e) {
    mRaw->setError(e.what());
    // We caught an exception reading WB, just ignore it
  }
  setMetaData(meta, mode, iso);
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
  if (hints.has("old_sraw_hue"))
    return (mRaw->metadata.subsampling.y * mRaw->metadata.subsampling.x);

  if (!mRootIFD->hasEntryRecursive(static_cast<TiffTag>(0x10))) {
    return 0;
  }
  if (uint32_t model_id =
          mRootIFD->getEntryRecursive(static_cast<TiffTag>(0x10))->getU32();
      model_id >= 0x80000281 || model_id == 0x80000218 ||
      (hints.has("force_new_sraw_hue")))
    return ((mRaw->metadata.subsampling.y * mRaw->metadata.subsampling.x) - 1) >> 1;

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
  sraw_coeffs[1] =
      (wb->getU16(offset + 1) + wb->getU16(offset + 2) + 1) >> 1;
  sraw_coeffs[2] = wb->getU16(offset + 3);

  if (hints.has("invert_sraw_wb")) {
    sraw_coeffs[0] = static_cast<int>(
        1024.0F / (static_cast<float>(sraw_coeffs[0]) / 1024.0F));
    sraw_coeffs[2] = static_cast<int>(
        1024.0F / (static_cast<float>(sraw_coeffs[2]) / 1024.0F));
  }

  mRaw->checkMemIsInitialized();
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
  bool isOldSraw = hints.has("sraw_40d");
  bool isNewSraw = hints.has("sraw_new");

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
}

} // namespace rawspeed
