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

#include "decoders/PefDecoder.h"
#include "adt/Array1DRef.h"
#include "adt/Array2DRef.h"
#include "adt/Casts.h"
#include "adt/Optional.h"
#include "adt/Point.h"
#include "bitstreams/BitStreams.h"
#include "common/Common.h"
#include "common/RawImage.h"
#include "decoders/RawDecoderException.h"
#include "decompressors/PentaxDecompressor.h"
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

bool PefDecoder::isAppropriateDecoder(const TiffRootIFD* rootIFD,
                                      [[maybe_unused]] Buffer file) {
  const auto id = rootIFD->getID();
  const std::string& make = id.make;

  // FIXME: magic

  return make == "PENTAX Corporation" ||
         make == "RICOH IMAGING COMPANY, LTD." || make == "PENTAX";
}

RawImage PefDecoder::decodeRawInternal() {
  const auto* raw = mRootIFD->getIFDWithTag(TiffTag::STRIPOFFSETS);

  int compression = raw->getEntry(TiffTag::COMPRESSION)->getU32();

  if (1 == compression || compression == 32773) {
    decodeUncompressed(raw, BitOrder::MSB);
    return mRaw;
  }

  if (65535 != compression)
    ThrowRDE("Unsupported compression");

  if (raw->hasEntry(TiffTag::PHOTOMETRICINTERPRETATION)) {
    mRaw->isCFA =
        (raw->getEntry(TiffTag::PHOTOMETRICINTERPRETATION)->getU16() != 34892);
  }

  if (mRaw->isCFA)
    writeLog(DEBUG_PRIO::EXTRA, "This is a CFA image");
  else {
    writeLog(DEBUG_PRIO::EXTRA, "This is NOT a CFA image");
  }

  const TiffEntry* offsets = raw->getEntry(TiffTag::STRIPOFFSETS);
  const TiffEntry* counts = raw->getEntry(TiffTag::STRIPBYTECOUNTS);

  if (offsets->count != 1) {
    ThrowRDE("Multiple Strips found: %u", offsets->count);
  }
  if (counts->count != offsets->count) {
    ThrowRDE(
        "Byte count number does not match strip size: count:%u, strips:%u ",
        counts->count, offsets->count);
  }
  ByteStream bs(
      DataBuffer(mFile.getSubView(offsets->getU32(), counts->getU32()),
                 Endianness::little));

  uint32_t width = raw->getEntry(TiffTag::IMAGEWIDTH)->getU32();
  uint32_t height = raw->getEntry(TiffTag::IMAGELENGTH)->getU32();

  mRaw->dim = iPoint2D(width, height);

  Optional<ByteStream> metaData;
  if (getRootIFD()->hasEntryRecursive(static_cast<TiffTag>(0x220))) {
    /* Attempt to read huffman table, if found in makernote */
    const TiffEntry* t =
        getRootIFD()->getEntryRecursive(static_cast<TiffTag>(0x220));
    if (t->type != TiffDataType::UNDEFINED)
      ThrowRDE("Unknown Huffman table type.");

    metaData = t->getData();
  }

  PentaxDecompressor p(mRaw, metaData);
  mRaw->createData();
  p.decompress(bs);

  return mRaw;
}

void PefDecoder::decodeMetaDataInternal(const CameraMetaData* meta) {
  int iso = 0;
  mRaw->cfa.setCFA(iPoint2D(2, 2), CFAColor::RED, CFAColor::GREEN,
                   CFAColor::GREEN, CFAColor::BLUE);

  if (mRootIFD->hasEntryRecursive(TiffTag::ISOSPEEDRATINGS))
    iso = mRootIFD->getEntryRecursive(TiffTag::ISOSPEEDRATINGS)->getU32();

  setMetaData(meta, "", iso);

  // Read black level
  if (mRootIFD->hasEntryRecursive(static_cast<TiffTag>(0x200))) {
    const TiffEntry* black =
        mRootIFD->getEntryRecursive(static_cast<TiffTag>(0x200));
    if (black->count == 4) {
      mRaw->blackLevelSeparate =
          Array2DRef(mRaw->blackLevelSeparateStorage.data(), 2, 2);
      auto blackLevelSeparate1D = *mRaw->blackLevelSeparate->getAsArray1DRef();
      for (int i = 0; i < 4; i++)
        blackLevelSeparate1D(i) = black->getU32(i);
    }
  }

  // Set the whitebalance
  if (mRootIFD->hasEntryRecursive(static_cast<TiffTag>(0x0201))) {
    const TiffEntry* wb =
        mRootIFD->getEntryRecursive(static_cast<TiffTag>(0x0201));
    if (wb->count == 4) {
      mRaw->metadata.wbCoeffs[0] = implicit_cast<float>(wb->getU32(0));
      mRaw->metadata.wbCoeffs[1] = implicit_cast<float>(wb->getU32(1));
      mRaw->metadata.wbCoeffs[2] = implicit_cast<float>(wb->getU32(3));
    }
  }
}

} // namespace rawspeed
