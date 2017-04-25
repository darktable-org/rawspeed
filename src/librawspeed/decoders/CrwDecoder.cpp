/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2014 Pedro Côrte-Real
    Copyright (C) 2015 Roman Lebedev

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

#include "decoders/CrwDecoder.h"
#include "common/Common.h"                 // for ushort16, uint32
#include "common/Point.h"                  // for iPoint2D
#include "common/RawspeedException.h"      // for RawspeedException
#include "decoders/RawDecoderException.h"  // for RawDecoderException (ptr ...
#include "decompressors/CrwDecompressor.h" // for CrwDecompressor
#include "metadata/Camera.h"               // for Hints
#include "metadata/ColorFilterArray.h"     // for CFAColor::CFA_GREEN, CFAC...
#include "tiff/CiffEntry.h"                // for CiffEntry, CiffDataType::...
#include "tiff/CiffIFD.h"                  // for CiffIFD
#include "tiff/CiffTag.h"                  // for CiffTag, CiffTag::CIFF_MA...
#include <algorithm>                       // for move
#include <cassert>                         // for assert
#include <cmath>                           // for copysignf, expf, logf
#include <cstdlib>                         // for abs
#include <memory>                          // for unique_ptr
#include <string>                          // for string
#include <vector>                          // for vector

using std::vector;
using std::string;
using std::abs;

namespace rawspeed {

class CameraMetaData;

CrwDecoder::CrwDecoder(std::unique_ptr<CiffIFD> rootIFD, Buffer* file)
    : RawDecoder(file), mRootIFD(move(rootIFD)) {}

RawImage CrwDecoder::decodeRawInternal() {
  CiffEntry *sensorInfo = mRootIFD->getEntryRecursive(CIFF_SENSORINFO);

  if (!sensorInfo || sensorInfo->count < 6 || sensorInfo->type != CIFF_SHORT)
    ThrowRDE("Couldn't find image sensor info");

  assert(sensorInfo != nullptr);
  uint32 width = sensorInfo->getU16(1);
  uint32 height = sensorInfo->getU16(2);

  CiffEntry *decTable = mRootIFD->getEntryRecursive(CIFF_DECODERTABLE);
  if (!decTable || decTable->type != CIFF_LONG)
    ThrowRDE("Couldn't find decoder table");

  assert(decTable != nullptr);
  uint32 dec_table = decTable->getU32();
  if (dec_table > 2)
    ThrowRDE("Unknown decoder table %d", dec_table);

  mRaw->dim = iPoint2D(width, height);
  mRaw->createData();

  bool lowbits = ! hints.has("no_decompressed_lowbits");
  CrwDecompressor::decompress(mRaw, mFile, dec_table, lowbits);

  return mRaw;
}

void CrwDecoder::checkSupportInternal(const CameraMetaData* meta) {
  vector<CiffIFD*> data = mRootIFD->getIFDsWithTag(CIFF_MAKEMODEL);
  if (data.empty())
    ThrowRDE("Model name not found");
  vector<string> makemodel = data[0]->getEntry(CIFF_MAKEMODEL)->getStrings();
  if (makemodel.size() < 2)
    ThrowRDE("wrong number of strings for make/model");
  string make = makemodel[0];
  string model = makemodel[1];

  this->checkCameraSupported(meta, make, model, "");
}

// based on exiftool's Image::ExifTool::Canon::CanonEv
float __attribute__((const)) CrwDecoder::canonEv(const long in) {
  // remove sign
  long val = abs(in);
  // remove fraction
  long frac = val & 0x1f;
  val -= frac;
  // convert 1/3 (0x0c) and 2/3 (0x14) codes
  if (frac == 0x0c) {
    frac = 32.0F / 3;
  }
  else if (frac == 0x14) {
    frac = 64.0F / 3;
  }
  return copysignf((val + frac) / 32.0F, in);
}

void CrwDecoder::decodeMetaDataInternal(const CameraMetaData* meta) {
  int iso = 0;
  mRaw->cfa.setCFA(iPoint2D(2,2), CFA_RED, CFA_GREEN, CFA_GREEN, CFA_BLUE);
  vector<CiffIFD*> data = mRootIFD->getIFDsWithTag(CIFF_MAKEMODEL);
  if (data.empty())
    ThrowRDE("Model name not found");
  vector<string> makemodel = data[0]->getEntry(CIFF_MAKEMODEL)->getStrings();
  if (makemodel.size() < 2)
    ThrowRDE("wrong number of strings for make/model");
  string make = makemodel[0];
  string model = makemodel[1];
  string mode;

  if (mRootIFD->hasEntryRecursive(CIFF_SHOTINFO)) {
    CiffEntry *shot_info = mRootIFD->getEntryRecursive(CIFF_SHOTINFO);
    if (shot_info->type == CIFF_SHORT && shot_info->count >= 2) {
      // os << exp(canonEv(value.toLong()) * log(2.0)) * 100.0 / 32.0;
      ushort16 iso_index = shot_info->getU16(2);
      iso = expf(canonEv(static_cast<long>(iso_index)) * logf(2.0)) * 100.0F /
            32.0F;
    }
  }

  // Fetch the white balance
  try{
    if (mRootIFD->hasEntryRecursive(static_cast<CiffTag>(0x0032))) {
      CiffEntry* wb = mRootIFD->getEntryRecursive(static_cast<CiffTag>(0x0032));
      if (wb->type == CIFF_BYTE && wb->count == 768) {
        // We're in a D30 file, values are RGGB
        // This will probably not get used anyway as a 0x102c tag should exist
        mRaw->metadata.wbCoeffs[0] =
            static_cast<float>(1024.0 / wb->getByte(72));
        mRaw->metadata.wbCoeffs[1] =
            static_cast<float>((1024.0 / wb->getByte(73)) +
                               (1024.0 / wb->getByte(74))) /
            2.0F;
        mRaw->metadata.wbCoeffs[2] =
            static_cast<float>(1024.0 / wb->getByte(75));
      } else if (wb->type == CIFF_BYTE && wb->count > 768) { // Other G series and S series cameras
        // correct offset for most cameras
        int offset = hints.get("wb_offset", 120);

        ushort16 key[] = { 0x410, 0x45f3 };
        if (! hints.has("wb_mangle"))
          key[0] = key[1] = 0;

        offset /= 2;
        mRaw->metadata.wbCoeffs[0] =
            static_cast<float>(wb->getU16(offset + 1) ^ key[1]);
        mRaw->metadata.wbCoeffs[1] =
            static_cast<float>(wb->getU16(offset + 0) ^ key[0]);
        mRaw->metadata.wbCoeffs[2] =
            static_cast<float>(wb->getU16(offset + 2) ^ key[0]);
      }
    }
    if (mRootIFD->hasEntryRecursive(static_cast<CiffTag>(0x102c))) {
      CiffEntry* entry =
          mRootIFD->getEntryRecursive(static_cast<CiffTag>(0x102c));
      if (entry->type == CIFF_SHORT && entry->getU16() > 512) {
        // G1/Pro90 CYGM pattern
        mRaw->metadata.wbCoeffs[0] = static_cast<float>(entry->getU16(62));
        mRaw->metadata.wbCoeffs[1] = static_cast<float>(entry->getU16(63));
        mRaw->metadata.wbCoeffs[2] = static_cast<float>(entry->getU16(60));
        mRaw->metadata.wbCoeffs[3] = static_cast<float>(entry->getU16(61));
      } else if (entry->type == CIFF_SHORT) {
        /* G2, S30, S40 */
        mRaw->metadata.wbCoeffs[0] = static_cast<float>(entry->getU16(51));
        mRaw->metadata.wbCoeffs[1] = (static_cast<float>(entry->getU16(50)) +
                                      static_cast<float>(entry->getU16(53))) /
                                     2.0F;
        mRaw->metadata.wbCoeffs[2] = static_cast<float>(entry->getU16(52));
      }
    }
    if (mRootIFD->hasEntryRecursive(CIFF_SHOTINFO) && mRootIFD->hasEntryRecursive(CIFF_WHITEBALANCE)) {
      CiffEntry *shot_info = mRootIFD->getEntryRecursive(CIFF_SHOTINFO);
      ushort16 wb_index = shot_info->getU16(7);
      CiffEntry *wb_data = mRootIFD->getEntryRecursive(CIFF_WHITEBALANCE);
      /* CANON EOS D60, CANON EOS 10D, CANON EOS 300D */
      if (wb_index > 9)
        ThrowRDE("Invalid white balance index");
      int wb_offset = 1 + ("0134567028"[wb_index]-'0') * 4;
      mRaw->metadata.wbCoeffs[0] = wb_data->getU16(wb_offset + 0);
      mRaw->metadata.wbCoeffs[1] = wb_data->getU16(wb_offset + 1);
      mRaw->metadata.wbCoeffs[2] = wb_data->getU16(wb_offset + 3);
    }
  } catch (RawspeedException& e) {
    mRaw->setError(e.what());
    // We caught an exception reading WB, just ignore it
  }

  setMetaData(meta, make, model, mode, iso);
}

} // namespace rawspeed
