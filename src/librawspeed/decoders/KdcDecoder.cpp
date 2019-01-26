/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
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

#include "decoders/KdcDecoder.h"
#include "common/Common.h"                          // for uint32, ushort16
#include "common/NORangesSet.h"                     // for NORangesSet
#include "common/Point.h"                           // for iPoint2D
#include "decoders/RawDecoderException.h"           // for RawDecoderExcept...
#include "decompressors/UncompressedDecompressor.h" // for UncompressedDeco...
#include "io/Buffer.h"                              // for Buffer
#include "io/Endianness.h"                          // for Endianness
#include "metadata/Camera.h"                        // for Hints
#include "parsers/TiffParserException.h"            // for TiffParserException
#include "tiff/TiffEntry.h"                         // for TiffEntry
#include "tiff/TiffIFD.h"                           // for TiffRootIFD
#include "tiff/TiffTag.h"                           // for TiffTag::COMPRES...
#include <cassert>                                  // for assert
#include <memory>                                   // for unique_ptr
#include <string>                                   // for operator==, string

namespace rawspeed {

bool KdcDecoder::isAppropriateDecoder(const TiffRootIFD* rootIFD,
                                      const Buffer* file) {
  const auto id = rootIFD->getID();
  const std::string& make = id.make;

  // FIXME: magic

  return make == "EASTMAN KODAK COMPANY";
}

Buffer KdcDecoder::getInputBuffer() {
  TiffEntry* offset = mRootIFD->getEntryRecursive(KODAK_KDC_OFFSET);
  if (!offset || offset->count < 13)
    ThrowRDE("Couldn't find the KDC offset");

  assert(offset != nullptr);
  uint64 off = uint64(offset->getU32(4)) + uint64(offset->getU32(12));
  if (off > std::numeric_limits<uint32>::max())
    ThrowRDE("Offset is too large.");

  // Offset hardcoding gotten from dcraw
  if (hints.has("easyshare_offset_hack"))
    off = off < 0x15000 ? 0x15000 : 0x17000;

  if (off > mFile->getSize())
    ThrowRDE("offset is out of bounds");

  const auto area = mRaw->dim.area();
  const auto bits = 12 * area;
  assert(bits % 8 == 0);
  const auto bytes = bits / 8;

  return mFile->getSubView(off, bytes);
}

RawImage KdcDecoder::decodeRawInternal() {
  if (!mRootIFD->hasEntryRecursive(COMPRESSION))
    ThrowRDE("Couldn't find compression setting");

  int compression = mRootIFD->getEntryRecursive(COMPRESSION)->getU32();
  if (7 != compression)
    ThrowRDE("Unsupported compression %d", compression);

  TiffEntry* ifdoffset = mRootIFD->getEntryRecursive(KODAK_IFD2);
  if (!ifdoffset)
    ThrowRDE("Couldn't find the Kodak IFD offset");

  NORangesSet<Buffer> ifds;

  assert(ifdoffset != nullptr);
  TiffRootIFD kodakifd(nullptr, &ifds, ifdoffset->getRootIfdData(),
                       ifdoffset->getU32());

  uint32 width = 0;
  uint32 height = 0;
  TiffEntry* ew = kodakifd.getEntryRecursive(KODAK_KDC_SENSOR_WIDTH);
  TiffEntry* eh = kodakifd.getEntryRecursive(KODAK_KDC_SENSOR_HEIGHT);
  if (ew && eh) {
    width = ew->getU32();
    height = eh->getU32();
  } else
    ThrowRDE("Unable to retrieve image size");

  mRaw->dim = iPoint2D(width, height);

  const Buffer inputBuffer = KdcDecoder::getInputBuffer();

  mRaw->createData();

  UncompressedDecompressor u(inputBuffer, mRaw);

  u.decode12BitRaw<Endianness::big>(width, height);

  return mRaw;
}

void KdcDecoder::decodeMetaDataInternal(const CameraMetaData* meta) {
  setMetaData(meta, "", 0);

  // Try the kodak hidden IFD for WB
  if (mRootIFD->hasEntryRecursive(KODAK_IFD2)) {
    TiffEntry *ifdoffset = mRootIFD->getEntryRecursive(KODAK_IFD2);
    try {
      NORangesSet<Buffer> ifds;

      TiffRootIFD kodakifd(nullptr, &ifds, ifdoffset->getRootIfdData(),
                           ifdoffset->getU32());

      if (kodakifd.hasEntryRecursive(KODAK_KDC_WB)) {
        TiffEntry *wb = kodakifd.getEntryRecursive(KODAK_KDC_WB);
        if (wb->count == 3) {
          mRaw->metadata.wbCoeffs[0] = wb->getFloat(0);
          mRaw->metadata.wbCoeffs[1] = wb->getFloat(1);
          mRaw->metadata.wbCoeffs[2] = wb->getFloat(2);
        }
      }
    } catch (TiffParserException &e) {
      mRaw->setError(e.what());
    }
  }

  // Use the normal WB if available
  if (mRootIFD->hasEntryRecursive(KODAKWB)) {
    TiffEntry *wb = mRootIFD->getEntryRecursive(KODAKWB);
    if (wb->count == 734 || wb->count == 1502) {
      mRaw->metadata.wbCoeffs[0] =
          static_cast<float>(((static_cast<ushort16>(wb->getByte(148))) << 8) |
                             wb->getByte(149)) /
          256.0F;
      mRaw->metadata.wbCoeffs[1] = 1.0F;
      mRaw->metadata.wbCoeffs[2] =
          static_cast<float>(((static_cast<ushort16>(wb->getByte(150))) << 8) |
                             wb->getByte(151)) /
          256.0F;
    }
  }
}

} // namespace rawspeed
