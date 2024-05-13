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

#include "decoders/MefDecoder.h"
#include "adt/Point.h"
#include "bitstreams/BitStreams.h"
#include "common/RawImage.h"
#include "decoders/RawDecoderException.h"
#include "decoders/SimpleTiffDecoder.h"
#include "decompressors/UncompressedDecompressor.h"
#include "io/Buffer.h"
#include "io/ByteStream.h"
#include "io/Endianness.h"
#include "tiff/TiffIFD.h"
#include <string>

namespace rawspeed {

bool MefDecoder::isAppropriateDecoder(const TiffRootIFD* rootIFD,
                                      [[maybe_unused]] Buffer file) {
  const auto id = rootIFD->getID();
  const std::string& make = id.make;

  // FIXME: magic

  return make == "Mamiya-OP Co.,Ltd.";
}

void MefDecoder::checkImageDimensions() {
  if (width > 4016 || height > 5344)
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", width, height);
}

RawImage MefDecoder::decodeRawInternal() {
  SimpleTiffDecoder::prepareForRawDecoding();

  UncompressedDecompressor u(
      ByteStream(DataBuffer(mFile.getSubView(off), Endianness::little)), mRaw,
      iRectangle2D({0, 0}, iPoint2D(width, height)), 12 * width / 8, 12,
      BitOrder::MSB);
  mRaw->createData();
  u.readUncompressedRaw();

  return mRaw;
}

void MefDecoder::decodeMetaDataInternal(const CameraMetaData* meta) {
  setMetaData(meta, "", 0);
}

} // namespace rawspeed
