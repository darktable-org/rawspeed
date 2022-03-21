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

#include "decoders/DcrDecoder.h"
#include "common/NORangesSet.h"              // for set
#include "decoders/RawDecoderException.h"    // for ThrowRDE
#include "decompressors/KodakDecompressor.h" // for KodakDecompressor
#include "io/Buffer.h"                       // for Buffer, DataBuffer
#include "io/ByteStream.h"                   // for ByteStream
#include "io/Endianness.h"                   // for Endianness, Endianness:...
#include "tiff/TiffEntry.h" // for TiffEntry, TiffDataType::SHORT
#include "tiff/TiffIFD.h"                    // for TiffRootIFD, TiffID
#include "tiff/TiffTag.h"                    // for COMPRESSION, KODAK_IFD
#include <array>                             // for array
#include <cassert>                           // for assert
#include <memory>                            // for unique_ptr
#include <string>                            // for operator==, string

namespace rawspeed {

class CameraMetaData;

bool DcrDecoder::isAppropriateDecoder(const TiffRootIFD* rootIFD,
                                      [[maybe_unused]] const Buffer& file) {
  const auto id = rootIFD->getID();
  const std::string& make = id.make;

  // FIXME: magic

  return make == "Kodak";
}

void DcrDecoder::checkImageDimensions() {
  if (width > 4516 || height > 3012)
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", width, height);
}

void DcrDecoder::decodeRawInternal() {
  SimpleTiffDecoder::prepareForRawDecoding();

  ByteStream input(DataBuffer(mFile.getSubView(off), Endianness::little));

  if (int compression = raw->getEntry(TiffTag::COMPRESSION)->getU32();
      65000 != compression)
    ThrowRDE("Unsupported compression %d", compression);

  const TiffEntry* ifdoffset = mRootIFD->getEntryRecursive(TiffTag::KODAK_IFD);
  if (!ifdoffset)
    ThrowRDE("Couldn't find the Kodak IFD offset");

  NORangesSet<Buffer> ifds;

  assert(ifdoffset != nullptr);
  TiffRootIFD kodakifd(nullptr, &ifds, ifdoffset->getRootIfdData(),
                       ifdoffset->getU32());

  const TiffEntry* linearization =
      kodakifd.getEntryRecursive(TiffTag::KODAK_LINEARIZATION);
  if (!linearization ||
      !(linearization->count == 1024 || linearization->count == 4096) ||
      linearization->type != TiffDataType::SHORT)
    ThrowRDE("Couldn't find the linearization table");

  assert(linearization != nullptr);
  auto linTable = linearization->getU16Array(linearization->count);

  RawImageCurveGuard curveHandler(mRaw.get(0).get(), linTable, uncorrectedRawValues);

  // FIXME: dcraw does all sorts of crazy things besides this to fetch
  //        WB from what appear to be presets and calculate it in weird ways
  //        The only file I have only uses this method, if anybody careas look
  //        in dcraw.c parse_kodak_ifd() for all that weirdness
  if (const TiffEntry* blob =
          kodakifd.getEntryRecursive(static_cast<TiffTag>(0x03fd));
      blob && blob->count == 72) {
    for (auto i = 0U; i < 3; i++) {
      const auto mul = blob->getU16(20 + i);
      if (0 == mul)
        ThrowRDE("WB coefficient is zero!");
      mRaw.get(0)->metadata.wbCoeffs[i] = 2048.0F / mul;
    }
  }

  const int bps = [CurveSize = linearization->count]() {
    switch (CurveSize) {
    case 1024:
      return 10;
    case 4096:
      return 12;
    default:
      __builtin_unreachable();
    }
  }();

  KodakDecompressor k(mRaw.get(0).get(), input, bps, uncorrectedRawValues);
  k.decompress();
}

void DcrDecoder::decodeMetaDataInternal(const CameraMetaData* meta) {
  setMetaData(meta, "", 0);
}

} // namespace rawspeed
