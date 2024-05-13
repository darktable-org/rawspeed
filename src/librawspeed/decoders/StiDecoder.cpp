/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2023 Roman Lebedev

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

#include "decoders/StiDecoder.h"
#include "adt/Point.h"
#include "bitstreams/BitStreams.h"
#include "common/RawImage.h"
#include "decoders/RawDecoderException.h"
#include "decompressors/HasselbladLJpegDecoder.h"
#include "decompressors/UncompressedDecompressor.h"
#include "io/Buffer.h"
#include "io/ByteStream.h"
#include "io/Endianness.h"
#include "tiff/TiffEntry.h"
#include "tiff/TiffIFD.h"
#include "tiff/TiffTag.h"
#include <cstdint>
#include <memory>
#include <string>

namespace rawspeed {

class CameraMetaData;

bool StiDecoder::isAppropriateDecoder(const TiffRootIFD* rootIFD,
                                      [[maybe_unused]] Buffer file) {
  const auto id = rootIFD->getID();
  const std::string& make = id.make;

  // FIXME: magic

  return make == "Sinar AG";
}

RawImage StiDecoder::decodeRawInternal() {
  const auto* raw = mRootIFD->getIFDWithTag(TiffTag::TILEOFFSETS, 0);
  uint32_t width = raw->getEntry(TiffTag::IMAGEWIDTH)->getU32();
  uint32_t height = raw->getEntry(TiffTag::IMAGELENGTH)->getU32();
  uint32_t compression = raw->getEntry(TiffTag::COMPRESSION)->getU32();

  mRaw->dim = iPoint2D(width, height);

  if (1 != compression)
    ThrowRDE("Unexpected compression type.");

  DecodeUncompressed(raw);
  return mRaw;
}

void StiDecoder::DecodeUncompressed(const TiffIFD* raw) const {
  if (mRaw->getDataType() != RawImageType::UINT16)
    ThrowRDE("Unexpected data type");

  if (mRaw->getCpp() != 1 || mRaw->getBpp() != sizeof(uint16_t))
    ThrowRDE("Unexpected cpp: %u", mRaw->getCpp());

  // FIXME: could be wrong.
  if (!mRaw->dim.hasPositiveArea() || mRaw->dim.x % 2 != 0 ||
      mRaw->dim.y % 2 != 0 || mRaw->dim.x > 4992 || mRaw->dim.y > 6668) {
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", mRaw->dim.x,
             mRaw->dim.y);
  }

  uint32_t off = raw->getEntry(TiffTag::TILEOFFSETS)->getU32();
  uint32_t count = raw->getEntry(TiffTag::TILEBYTECOUNTS)->getU32();

  const ByteStream bs(
      DataBuffer(mFile.getSubView(off, count), Endianness::little));

  UncompressedDecompressor u(bs, mRaw, iRectangle2D({0, 0}, mRaw->dim),
                             2 * mRaw->dim.x, 16, BitOrder::MSB);
  mRaw->createData();
  u.readUncompressedRaw();
}

void StiDecoder::decodeMetaDataInternal(const CameraMetaData* meta) {
  setMetaData(meta, "", 0);
}

} // namespace rawspeed
