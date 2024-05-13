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

#include "decoders/ThreefrDecoder.h"
#include "adt/Casts.h"
#include "adt/Point.h"
#include "bitstreams/BitStreams.h"
#include "common/RawImage.h"
#include "decoders/RawDecoderException.h"
#include "decompressors/HasselbladLJpegDecoder.h"
#include "decompressors/UncompressedDecompressor.h"
#include "io/Buffer.h"
#include "io/ByteStream.h"
#include "io/Endianness.h"
#include "metadata/ColorFilterArray.h"
#include "tiff/TiffEntry.h"
#include "tiff/TiffIFD.h"
#include "tiff/TiffTag.h"
#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace rawspeed {

class CameraMetaData;

bool ThreefrDecoder::isAppropriateDecoder(const TiffRootIFD* rootIFD,
                                          [[maybe_unused]] Buffer file) {
  const auto id = rootIFD->getID();
  const std::string& make = id.make;

  // FIXME: magic

  return make == "Hasselblad";
}

RawImage ThreefrDecoder::decodeRawInternal() {
  const auto* raw = mRootIFD->getIFDWithTag(TiffTag::STRIPOFFSETS, 1);
  uint32_t width = raw->getEntry(TiffTag::IMAGEWIDTH)->getU32();
  uint32_t height = raw->getEntry(TiffTag::IMAGELENGTH)->getU32();
  uint32_t compression = raw->getEntry(TiffTag::COMPRESSION)->getU32();

  mRaw->dim = iPoint2D(width, height);

  if (1 == compression) {
    DecodeUncompressed(raw);
    return mRaw;
  }

  if (compression != 7) // LJpeg
    ThrowRDE("Unexpected compression type.");

  uint32_t off = raw->getEntry(TiffTag::STRIPOFFSETS)->getU32();
  // STRIPBYTECOUNTS is strange/invalid for the existing (compressed?) 3FR
  // samples...

  const ByteStream bs(DataBuffer(mFile.getSubView(off), Endianness::little));

  HasselbladLJpegDecoder l(bs, mRaw);
  mRaw->createData();

  l.decode();

  return mRaw;
}

void ThreefrDecoder::DecodeUncompressed(const TiffIFD* raw) const {
  if (mRaw->getDataType() != RawImageType::UINT16)
    ThrowRDE("Unexpected data type");

  if (mRaw->getCpp() != 1 || mRaw->getBpp() != sizeof(uint16_t))
    ThrowRDE("Unexpected cpp: %u", mRaw->getCpp());

  // FIXME: could be wrong. max "active pixels" - "100 MP"
  if (!mRaw->dim.hasPositiveArea() || mRaw->dim.x % 2 != 0 ||
      mRaw->dim.x > 12000 || mRaw->dim.y > 8842) {
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", mRaw->dim.x,
             mRaw->dim.y);
  }

  uint32_t off = raw->getEntry(TiffTag::STRIPOFFSETS)->getU32();
  // STRIPBYTECOUNTS looks valid for the existing uncompressed 3FR samples
  uint32_t count = raw->getEntry(TiffTag::STRIPBYTECOUNTS)->getU32();

  const ByteStream bs(
      DataBuffer(mFile.getSubView(off, count), Endianness::little));

  UncompressedDecompressor u(bs, mRaw, iRectangle2D({0, 0}, mRaw->dim),
                             2 * mRaw->dim.x, 16, BitOrder::LSB);
  mRaw->createData();
  u.readUncompressedRaw();
}

void ThreefrDecoder::decodeMetaDataInternal(const CameraMetaData* meta) {
  mRaw->cfa.setCFA(iPoint2D(2, 2), CFAColor::RED, CFAColor::GREEN,
                   CFAColor::GREEN, CFAColor::BLUE);

  setMetaData(meta, "", 0);

  if (mRootIFD->hasEntryRecursive(TiffTag::BLACKLEVEL)) {
    const TiffEntry* bl = mRootIFD->getEntryRecursive(TiffTag::BLACKLEVEL);
    if (bl->count == 1)
      mRaw->blackLevel = implicit_cast<int>(bl->getFloat());
  }

  if (mRootIFD->hasEntryRecursive(TiffTag::WHITELEVEL)) {
    const TiffEntry* wl = mRootIFD->getEntryRecursive(TiffTag::WHITELEVEL);
    if (wl->count == 1)
      mRaw->whitePoint = implicit_cast<int>(wl->getFloat());
  }

  // Fetch the white balance
  if (mRootIFD->hasEntryRecursive(TiffTag::ASSHOTNEUTRAL)) {
    const TiffEntry* wb = mRootIFD->getEntryRecursive(TiffTag::ASSHOTNEUTRAL);
    if (wb->count == 3) {
      for (uint32_t i = 0; i < 3; i++) {
        const float div = wb->getFloat(i);
        if (div == 0.0F)
          ThrowRDE("Can not decode WB, multiplier is zero/");

        mRaw->metadata.wbCoeffs[i] = 1.0F / div;
      }
    }
  }
}

} // namespace rawspeed
