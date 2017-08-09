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
#include "common/Common.h"                    // for uint32, BitOrder::BitO...
#include "common/Point.h"                     // for iPoint2D
#include "decoders/RawDecoderException.h"     // for RawDecoderException (p...
#include "decompressors/PentaxDecompressor.h" // for PentaxDecompressor
#include "io/Buffer.h"                        // for Buffer
#include "io/ByteStream.h"                    // for ByteStream
#include "io/IOException.h"                   // for IOException
#include "metadata/ColorFilterArray.h"        // for CFAColor::CFA_GREEN
#include "tiff/TiffEntry.h"                   // for TiffEntry
#include "tiff/TiffIFD.h"                     // for TiffRootIFD, TiffIFD
#include "tiff/TiffTag.h"                     // for TiffTag, TiffTag::ISOS...
#include <memory>                             // for unique_ptr
#include <string>                             // for operator==, string

namespace rawspeed {

bool PefDecoder::isAppropriateDecoder(const TiffRootIFD* rootIFD,
                                      const Buffer* file) {
  const auto id = rootIFD->getID();
  const std::string& make = id.make;

  // FIXME: magic

  return make == "PENTAX Corporation" ||
         make == "RICOH IMAGING COMPANY, LTD." || make == "PENTAX";
}

RawImage PefDecoder::decodeRawInternal() {
  auto raw = mRootIFD->getIFDWithTag(STRIPOFFSETS);

  int compression = raw->getEntry(COMPRESSION)->getU32();

  if (1 == compression || compression == 32773) {
    decodeUncompressed(raw, BitOrder_MSB);
    return mRaw;
  }

  if (65535 != compression)
    ThrowRDE("Unsupported compression");

  TiffEntry *offsets = raw->getEntry(STRIPOFFSETS);
  TiffEntry *counts = raw->getEntry(STRIPBYTECOUNTS);

  if (offsets->count != 1) {
    ThrowRDE("Multiple Strips found: %u", offsets->count);
  }
  if (counts->count != offsets->count) {
    ThrowRDE(
        "Byte count number does not match strip size: count:%u, strips:%u ",
        counts->count, offsets->count);
  }

  ByteStream bs(mFile, offsets->getU32(), counts->getU32());

  uint32 width = raw->getEntry(IMAGEWIDTH)->getU32();
  uint32 height = raw->getEntry(IMAGELENGTH)->getU32();

  if (!width || !height || width % 2 != 0)
    ThrowRDE("Incorrect image dimensions (%u, %u).", width, height);

  mRaw->dim = iPoint2D(width, height);
  try {
    PentaxDecompressor p(bs, mRaw, getRootIFD());
    mRaw->createData();
    p.decompress();
  } catch (IOException& e) {
    mRaw->setError(e.what());
    // Let's ignore it, it may have delivered somewhat useful data.
  }

  return mRaw;
}

void PefDecoder::decodeMetaDataInternal(const CameraMetaData* meta) {
  int iso = 0;
  mRaw->cfa.setCFA(iPoint2D(2,2), CFA_RED, CFA_GREEN, CFA_GREEN, CFA_BLUE);

  if (mRootIFD->hasEntryRecursive(ISOSPEEDRATINGS))
    iso = mRootIFD->getEntryRecursive(ISOSPEEDRATINGS)->getU32();

  setMetaData(meta, "", iso);

  // Read black level
  if (mRootIFD->hasEntryRecursive(static_cast<TiffTag>(0x200))) {
    TiffEntry* black = mRootIFD->getEntryRecursive(static_cast<TiffTag>(0x200));
    if (black->count == 4) {
      for (int i = 0; i < 4; i++)
        mRaw->blackLevelSeparate[i] = black->getU32(i);
    }
  }

  // Set the whitebalance
  if (mRootIFD->hasEntryRecursive(static_cast<TiffTag>(0x0201))) {
    TiffEntry* wb = mRootIFD->getEntryRecursive(static_cast<TiffTag>(0x0201));
    if (wb->count == 4) {
      mRaw->metadata.wbCoeffs[0] = wb->getU32(0);
      mRaw->metadata.wbCoeffs[1] = wb->getU32(1);
      mRaw->metadata.wbCoeffs[2] = wb->getU32(3);
    }
  }
}

} // namespace rawspeed
