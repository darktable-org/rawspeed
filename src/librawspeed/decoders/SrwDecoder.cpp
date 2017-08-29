/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2010 Klaus Post
    Copyright (C) 2014-2015 Pedro Côrte-Real

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

#include "decoders/SrwDecoder.h"
#include "common/Common.h"                       // for BitOrder::BitOrder_LSB
#include "decoders/RawDecoderException.h"        // for ThrowRDE
#include "decompressors/SamsungV0Decompressor.h" // for SamsungV0Decompressor
#include "decompressors/SamsungV1Decompressor.h" // for SamsungV1Decompressor
#include "decompressors/SamsungV2Decompressor.h" // for SamsungV2Decompressor
#include "metadata/Camera.h"                     // for Hints
#include "metadata/CameraMetaData.h"             // for CameraMetaData
#include "tiff/TiffEntry.h"                      // for TiffEntry
#include "tiff/TiffIFD.h"                        // for TiffRootIFD, TiffIFD
#include "tiff/TiffTag.h"                        // for TiffTag::BITSPERSAMPLE
#include <memory>                                // for unique_ptr
#include <sstream>                               // for operator<<, ostring...
#include <string>                                // for string, operator==
#include <vector>                                // for vector

namespace rawspeed {

bool SrwDecoder::isAppropriateDecoder(const TiffRootIFD* rootIFD,
                                      const Buffer* file) {
  const auto id = rootIFD->getID();
  const std::string& make = id.make;

  // FIXME: magic

  return make == "SAMSUNG";
}

RawImage SrwDecoder::decodeRawInternal() {
  auto raw = mRootIFD->getIFDWithTag(STRIPOFFSETS);

  int compression = raw->getEntry(COMPRESSION)->getU32();
  const int bits = raw->getEntry(BITSPERSAMPLE)->getU32();

  if (12 != bits && 14 != bits)
    ThrowRDE("Unsupported bits per sample");

  if (32769 != compression && 32770 != compression && 32772 != compression && 32773 != compression)
    ThrowRDE("Unsupported compression");

  if (32769 == compression)
  {
    bool bit_order = hints.get("msb_override", false);
    this->decodeUncompressed(raw, bit_order ? BitOrder_MSB : BitOrder_LSB);
    return mRaw;
  }

  if (32770 == compression)
  {
    if (!raw->hasEntry(static_cast<TiffTag>(40976))) {
      bool bit_order = hints.get("msb_override", bits == 12);
      this->decodeUncompressed(raw, bit_order ? BitOrder_MSB : BitOrder_LSB);
      return mRaw;
    }
    uint32 nslices = raw->getEntry(STRIPOFFSETS)->count;
    if (nslices != 1)
      ThrowRDE("Only one slice supported, found %u", nslices);

    decodeCompressed(raw);
    return mRaw;
  }
  if (32772 == compression)
  {
    uint32 nslices = raw->getEntry(STRIPOFFSETS)->count;
    if (nslices != 1)
      ThrowRDE("Only one slice supported, found %u", nslices);

    decodeCompressed2(raw, bits);
    return mRaw;
  }
  if (32773 == compression)
  {
    decodeCompressed3(raw, bits);
    return mRaw;
  }
  ThrowRDE("Unsupported compression");
}

// Decoder for compressed srw files (NX300 and later)
void SrwDecoder::decodeCompressed( const TiffIFD* raw )
{
  SamsungV0Decompressor s0(mRaw, raw, mFile);
  s0.decompress();
}

// Decoder for compressed srw files (NX3000 and later)
void SrwDecoder::decodeCompressed2( const TiffIFD* raw, int bits)
{
  SamsungV1Decompressor s1(mRaw, raw, mFile, bits);
  s1.decompress();
}

// Decoder for third generation compressed SRW files (NX1)
void SrwDecoder::decodeCompressed3(const TiffIFD* raw, int bits)
{
  SamsungV2Decompressor s2(mRaw, raw, mFile, bits);
  s2.decompress();
}

std::string SrwDecoder::getMode() {
  std::vector<const TiffIFD*> data = mRootIFD->getIFDsWithTag(CFAPATTERN);
  std::ostringstream mode;
  if (!data.empty() && data[0]->hasEntryRecursive(BITSPERSAMPLE)) {
    mode << data[0]->getEntryRecursive(BITSPERSAMPLE)->getU32() << "bit";
    return mode.str();
  }
  return "";
}

void SrwDecoder::checkSupportInternal(const CameraMetaData* meta) {
  auto id = mRootIFD->getID();
  std::string mode = getMode();
  if (meta->hasCamera(id.make, id.model, mode))
    this->checkCameraSupported(meta, id, getMode());
  else
    this->checkCameraSupported(meta, id, "");
}

void SrwDecoder::decodeMetaDataInternal(const CameraMetaData* meta) {
  int iso = 0;
  if (mRootIFD->hasEntryRecursive(ISOSPEEDRATINGS))
    iso = mRootIFD->getEntryRecursive(ISOSPEEDRATINGS)->getU32();

  auto id = mRootIFD->getID();
  std::string mode = getMode();
  if (meta->hasCamera(id.make, id.model, mode))
    setMetaData(meta, id, mode, iso);
  else
    setMetaData(meta, id, "", iso);

  // Set the whitebalance
  if (mRootIFD->hasEntryRecursive(SAMSUNG_WB_RGGBLEVELSUNCORRECTED) &&
      mRootIFD->hasEntryRecursive(SAMSUNG_WB_RGGBLEVELSBLACK)) {
    TiffEntry *wb_levels = mRootIFD->getEntryRecursive(SAMSUNG_WB_RGGBLEVELSUNCORRECTED);
    TiffEntry *wb_black = mRootIFD->getEntryRecursive(SAMSUNG_WB_RGGBLEVELSBLACK);
    if (wb_levels->count == 4 && wb_black->count == 4) {
      mRaw->metadata.wbCoeffs[0] = wb_levels->getFloat(0) - wb_black->getFloat(0);
      mRaw->metadata.wbCoeffs[1] = wb_levels->getFloat(1) - wb_black->getFloat(1);
      mRaw->metadata.wbCoeffs[2] = wb_levels->getFloat(3) - wb_black->getFloat(3);
    }
  }
}

} // namespace rawspeed
