/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2015 Pedro Côrte-Real

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

#include "decoders/DcsDecoder.h"
#include "adt/Point.h"
#include "bitstreams/BitStreams.h"
#include "common/RawImage.h"
#include "decoders/RawDecoderException.h"
#include "decoders/SimpleTiffDecoder.h"
#include "decompressors/UncompressedDecompressor.h"
#include "io/Buffer.h"
#include "io/ByteStream.h"
#include "io/Endianness.h"
#include "tiff/TiffEntry.h"
#include "tiff/TiffIFD.h"
#include "tiff/TiffTag.h"
#include <cassert>
#include <memory>
#include <string>

namespace rawspeed {

class CameraMetaData;

bool DcsDecoder::isAppropriateDecoder(const TiffRootIFD* rootIFD,
                                      [[maybe_unused]] Buffer file) {
  const auto id = rootIFD->getID();
  const std::string& make = id.make;

  // FIXME: magic

  return make == "KODAK";
}

void DcsDecoder::checkImageDimensions() {
  if (width > 3072 || height > 2048)
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", width, height);
}

RawImage DcsDecoder::decodeRawInternal() {
  SimpleTiffDecoder::prepareForRawDecoding();

  const TiffEntry* linearization =
      mRootIFD->getEntryRecursive(TiffTag::GRAYRESPONSECURVE);
  if (!linearization || linearization->count != 256 ||
      linearization->type != TiffDataType::SHORT)
    ThrowRDE("Couldn't find the linearization table");

  assert(linearization != nullptr);
  auto table = linearization->getU16Array(256);

  RawImageCurveGuard curveHandler(&mRaw, table, uncorrectedRawValues);

  UncompressedDecompressor u(
      ByteStream(DataBuffer(mFile.getSubView(off, c2), Endianness::little)),
      mRaw, iRectangle2D({0, 0}, iPoint2D(width, height)), 8 * width / 8, 8,
      BitOrder::LSB);
  mRaw->createData();

  if (uncorrectedRawValues)
    u.decode8BitRaw<true>();
  else
    u.decode8BitRaw<false>();

  return mRaw;
}

void DcsDecoder::decodeMetaDataInternal(const CameraMetaData* meta) {
  setMetaData(meta, "", 0);
}

} // namespace rawspeed
