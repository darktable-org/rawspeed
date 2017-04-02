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
#include "common/Common.h"                 // for ushort16, clampBits, uint32
#include "common/Point.h"                  // for iPoint2D
#include "decoders/RawDecoderException.h"  // for RawDecoderException, Thro...
#include "decompressors/Cr2Decompressor.h" // for Cr2Decompressor
#include "interpolators/Cr2sRawInterpolator.h" // for Cr2sRawInterpolator
#include "io/Buffer.h"                         // for Buffer
#include "io/ByteStream.h"                     // for ByteStream
#include "io/Endianness.h"               // for getHostEndianness, Endian...
#include "io/IOException.h"              // for IOException
#include "metadata/Camera.h"             // for Hints
#include "metadata/ColorFilterArray.h"   // for CFAColor::CFA_GREEN, CFAC...
#include "parsers/TiffParserException.h" // for ThrowTPE
#include "tiff/TiffEntry.h"              // for TiffEntry, TiffDataType::...
#include "tiff/TiffTag.h"                // for TiffTag, TiffTag::CANONCO...
#include <array>                         // for array
#include <cassert>                       // for assert
#include <exception>                     // for exception
#include <memory>                        // for unique_ptr, allocator
#include <string>                        // for string
#include <vector>                        // for vector
// IWYU pragma: no_include <ext/alloc_traits.h>

using std::string;
using std::vector;

namespace RawSpeed {

RawImage Cr2Decoder::decodeOldFormat() {
  uint32 offset = 0;
  if (mRootIFD->getEntryRecursive(CANON_RAW_DATA_OFFSET))
    offset = mRootIFD->getEntryRecursive(CANON_RAW_DATA_OFFSET)->getU32();
  else {
    // D2000 is oh so special...
    auto ifd = mRootIFD->getIFDWithTag(CFAPATTERN);
    if (! ifd->hasEntry(STRIPOFFSETS))
      ThrowRDE("Couldn't find offset");

    offset = ifd->getEntry(STRIPOFFSETS)->getU32();
  }

  ByteStream b(mFile, offset+41, getHostEndianness() == big);
  int height = b.getU16();
  int width = b.getU16();

  // some old models (1D/1DS/D2000C) encode two lines as one
  // see: FIX_CANON_HALF_HEIGHT_DOUBLE_WIDTH
  if (width > 2*height) {
    height *= 2;
    width /= 2;
  }
  width *= 2; // components

  mRaw = RawImage::create({width, height});

  Cr2Decompressor l(*mFile, offset, mRaw);
  try {
    l.decode({width});
  } catch (IOException& e) {
    mRaw->setError(e.what());
  }

  // deal with D2000 GrayResponseCurve
  TiffEntry* curve = mRootIFD->getEntryRecursive((TiffTag)0x123);
  if (curve && curve->type == TIFF_SHORT && curve->count == 4096) {
    auto table = curve->getU16Array(curve->count);
    if (!uncorrectedRawValues) {
      mRaw->setTable(table.data(), table.size(), true);
      // Apply table
      mRaw->sixteenBitLookup();
      // Delete table
      mRaw->setTable(nullptr);
    } else {
      // We want uncorrected, but we store the table.
      mRaw->setTable(table.data(), table.size(), false);
    }
  }

  return mRaw;
}

// for technical details about Cr2 mRAW/sRAW, see http://lclevy.free.fr/cr2/

RawImage Cr2Decoder::decodeNewFormat() {
  TiffEntry* sensorInfoE = mRootIFD->getEntryRecursive(CANON_SENSOR_INFO);
  if (!sensorInfoE)
    ThrowTPE("failed to get SensorInfo from MakerNote");

  assert(sensorInfoE != nullptr);
  iPoint2D dim(sensorInfoE->getU16(1), sensorInfoE->getU16(2));

  int componentsPerPixel = 1;
  TiffIFD* raw = mRootIFD->getSubIFDs()[3].get();
  if (raw->hasEntry(CANON_SRAWTYPE) &&
      raw->getEntry(CANON_SRAWTYPE)->getU32() == 4)
    componentsPerPixel = 3;

  mRaw = RawImage::create(dim, TYPE_USHORT16, componentsPerPixel);

  vector<int> s_width;
  TiffEntry* cr2SliceEntry = raw->getEntryRecursive(CANONCR2SLICE);
  if (cr2SliceEntry && cr2SliceEntry->getU16(0) > 0) {
    for (int i = 0; i < cr2SliceEntry->getU16(0); i++)
      s_width.push_back(cr2SliceEntry->getU16(1));
    s_width.push_back(cr2SliceEntry->getU16(2));
  }

  TiffEntry* offsets = raw->getEntry(STRIPOFFSETS);
  TiffEntry* counts = raw->getEntry(STRIPBYTECOUNTS);

  const uint32 byteOffset = offsets->getU32();
  const uint32 byteCount = counts->getU32();

  if (!mFile->isValid(byteOffset, byteCount))
    ThrowRDE("Strip is larger than the data; image may be truncated.");

  Cr2Decompressor d(*mFile, byteOffset, byteCount, mRaw);

  try {
    d.decode(s_width);
  } catch (RawDecoderException &e) {
    mRaw->setError(e.what());
  } catch (IOException &e) {
    // Let's try to ignore this - it might be truncated data, so something might be useful.
    mRaw->setError(e.what());
  }

  if (mRaw->metadata.subsampling.x > 1 || mRaw->metadata.subsampling.y > 1)
    sRawInterpolate();

  return mRaw;
}

RawImage Cr2Decoder::decodeRawInternal() {
  if (mRootIFD->getSubIFDs().size() < 4)
    return decodeOldFormat();
  else // NOLINT ok, here it make sense
    return decodeNewFormat();
}

void Cr2Decoder::checkSupportInternal(const CameraMetaData* meta) {
  auto id = mRootIFD->getID();
  // Check for sRaw mode
  if (mRootIFD->getSubIFDs().size() == 4) {
    TiffEntry* typeE = mRootIFD->getSubIFDs()[3]->getEntryRecursive(CANON_SRAWTYPE);
    if (typeE && typeE->getU32() == 4) {
      checkCameraSupported(meta, id, "sRaw1");
      return;
    }
  }

  checkCameraSupported(meta, id, "");
}

void Cr2Decoder::decodeMetaDataInternal(const CameraMetaData* meta) {
  int iso = 0;
  mRaw->cfa.setCFA(iPoint2D(2,2), CFA_RED, CFA_GREEN, CFA_GREEN, CFA_BLUE);

  string mode;

  if (mRaw->metadata.subsampling.y == 2 && mRaw->metadata.subsampling.x == 2)
    mode = "sRaw1";

  if (mRaw->metadata.subsampling.y == 1 && mRaw->metadata.subsampling.x == 2)
    mode = "sRaw2";

  if (mRootIFD->hasEntryRecursive(ISOSPEEDRATINGS))
    iso = mRootIFD->getEntryRecursive(ISOSPEEDRATINGS)->getU32();

  // Fetch the white balance
  try{
    if (mRootIFD->hasEntryRecursive(CANONCOLORDATA)) {
      TiffEntry *wb = mRootIFD->getEntryRecursive(CANONCOLORDATA);
      // this entry is a big table, and different cameras store used WB in
      // different parts, so find the offset, default is the most common one
      int offset = hints.get("wb_offset", 126);

      offset /= 2;
      mRaw->metadata.wbCoeffs[0] = (float) wb->getU16(offset + 0);
      mRaw->metadata.wbCoeffs[1] = (float) wb->getU16(offset + 1);
      mRaw->metadata.wbCoeffs[2] = (float) wb->getU16(offset + 3);
    } else {
      if (mRootIFD->hasEntryRecursive(CANONSHOTINFO) &&
          mRootIFD->hasEntryRecursive(CANONPOWERSHOTG9WB)) {
        TiffEntry *shot_info = mRootIFD->getEntryRecursive(CANONSHOTINFO);
        TiffEntry *g9_wb = mRootIFD->getEntryRecursive(CANONPOWERSHOTG9WB);

        ushort16 wb_index = shot_info->getU16(7);
        int wb_offset = (wb_index < 18) ? "012347800000005896"[wb_index]-'0' : 0;
        wb_offset = wb_offset*8 + 2;

        mRaw->metadata.wbCoeffs[0] = (float) g9_wb->getU32(wb_offset+1);
        mRaw->metadata.wbCoeffs[1] = ((float) g9_wb->getU32(wb_offset+0) + (float) g9_wb->getU32(wb_offset+3)) / 2.0f;
        mRaw->metadata.wbCoeffs[2] = (float) g9_wb->getU32(wb_offset+2);
      } else if (mRootIFD->hasEntryRecursive((TiffTag) 0xa4)) {
        // WB for the old 1D and 1DS
        TiffEntry *wb = mRootIFD->getEntryRecursive((TiffTag) 0xa4);
        if (wb->count >= 3) {
          mRaw->metadata.wbCoeffs[0] = wb->getFloat(0);
          mRaw->metadata.wbCoeffs[1] = wb->getFloat(1);
          mRaw->metadata.wbCoeffs[2] = wb->getFloat(2);
        }
      }
    }
  } catch (const std::exception& e) {
    mRaw->setError(e.what());
    // We caught an exception reading WB, just ignore it
  }
  setMetaData(meta, mode, iso);
}

int Cr2Decoder::getHue() {
  if (hints.has("old_sraw_hue"))
    return (mRaw->metadata.subsampling.y * mRaw->metadata.subsampling.x);

  if (!mRootIFD->hasEntryRecursive((TiffTag)0x10)) {
    return 0;
  }
  uint32 model_id = mRootIFD->getEntryRecursive((TiffTag)0x10)->getU32();
  if (model_id >= 0x80000281 || model_id == 0x80000218 || (hints.has("force_new_sraw_hue")))
    return ((mRaw->metadata.subsampling.y * mRaw->metadata.subsampling.x) - 1) >> 1;

  return (mRaw->metadata.subsampling.y * mRaw->metadata.subsampling.x);
}

// Interpolate and convert sRaw data.
void Cr2Decoder::sRawInterpolate() {
  TiffEntry* wb = mRootIFD->getEntryRecursive(CANONCOLORDATA);
  if (!wb)
    ThrowRDE("Unable to locate WB info.");

  // Offset to sRaw coefficients used to reconstruct uncorrected RGB data.
  uint32 offset = 78;

  std::array<int, 3> sraw_coeffs;

  assert(wb != nullptr);
  sraw_coeffs[0] = wb->getU16(offset + 0);
  sraw_coeffs[1] =
      (wb->getU16(offset + 1) + wb->getU16(offset + 2) + 1) >> 1;
  sraw_coeffs[2] = wb->getU16(offset + 3);

  if (hints.has("invert_sraw_wb")) {
    sraw_coeffs[0] = (int)(1024.0f / ((float)sraw_coeffs[0] / 1024.0f));
    sraw_coeffs[2] = (int)(1024.0f / ((float)sraw_coeffs[2] / 1024.0f));
  }

  /* Determine sRaw coefficients */
  bool isOldSraw = hints.has("sraw_40d");
  bool isNewSraw = hints.has("sraw_new");

  Cr2sRawInterpolator i(mRaw, sraw_coeffs, getHue());

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

} // namespace RawSpeed
