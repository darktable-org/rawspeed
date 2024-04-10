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
#include "adt/Casts.h"
#include "adt/NORangesSet.h"
#include "adt/Point.h"
#include "bitstreams/BitStreams.h"
#include "common/RawImage.h"
#include "decoders/RawDecoderException.h"
#include "decompressors/UncompressedDecompressor.h"
#include "io/Buffer.h"
#include "io/ByteStream.h"
#include "io/Endianness.h"
#include "metadata/Camera.h"
#include "parsers/TiffParserException.h"
#include "tiff/TiffEntry.h"
#include "tiff/TiffIFD.h"
#include "tiff/TiffTag.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>

namespace rawspeed {

bool KdcDecoder::isAppropriateDecoder(const TiffRootIFD* rootIFD,
                                      [[maybe_unused]] Buffer file) {
  const auto id = rootIFD->getID();
  const std::string& make = id.make;

  // FIXME: magic

  return make == "EASTMAN KODAK COMPANY";
}

Buffer KdcDecoder::getInputBuffer() const {
  const TiffEntry* offset =
      mRootIFD->getEntryRecursive(TiffTag::KODAK_KDC_OFFSET);
  if (!offset || offset->count < 13)
    ThrowRDE("Couldn't find the KDC offset");

  assert(offset != nullptr);
  uint64_t off = uint64_t(offset->getU32(4)) + uint64_t(offset->getU32(12));
  if (off > std::numeric_limits<uint32_t>::max())
    ThrowRDE("Offset is too large.");

  // Offset hardcoding gotten from dcraw
  if (hints.contains("easyshare_offset_hack"))
    off = off < 0x15000 ? 0x15000 : 0x17000;

  return mFile.getSubView(implicit_cast<Buffer::size_type>(off));
}

RawImage KdcDecoder::decodeRawInternal() {
  if (!mRootIFD->hasEntryRecursive(TiffTag::COMPRESSION))
    ThrowRDE("Couldn't find compression setting");

  if (auto compression =
          mRootIFD->getEntryRecursive(TiffTag::COMPRESSION)->getU32();
      7 != compression)
    ThrowRDE("Unsupported compression %d", compression);

  const TiffEntry* ifdoffset = mRootIFD->getEntryRecursive(TiffTag::KODAK_IFD2);
  if (!ifdoffset)
    ThrowRDE("Couldn't find the Kodak IFD offset");

  NORangesSet<Buffer> ifds;

  assert(ifdoffset != nullptr);
  TiffRootIFD kodakifd(nullptr, &ifds, ifdoffset->getRootIfdData(),
                       ifdoffset->getU32());

  const TiffEntry* ew =
      kodakifd.getEntryRecursive(TiffTag::KODAK_KDC_SENSOR_WIDTH);
  const TiffEntry* eh =
      kodakifd.getEntryRecursive(TiffTag::KODAK_KDC_SENSOR_HEIGHT);
  if (!ew || !eh)
    ThrowRDE("Unable to retrieve image size");

  uint32_t width = ew->getU32();
  uint32_t height = eh->getU32();

  mRaw->dim = iPoint2D(width, height);

  if (!mRaw->dim.hasPositiveArea() || !(mRaw->dim <= iPoint2D(4304, 3221)))
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", mRaw->dim.x,
             mRaw->dim.y);

  const Buffer inputBuffer = KdcDecoder::getInputBuffer();

  UncompressedDecompressor u(
      ByteStream(DataBuffer(inputBuffer, Endianness::little)), mRaw,
      iRectangle2D({0, 0}, iPoint2D(width, height)), 12 * width / 8, 12,
      BitOrder::MSB);
  mRaw->createData();
  u.readUncompressedRaw();

  return mRaw;
}

void KdcDecoder::decodeMetaDataInternal(const CameraMetaData* meta) {
  setMetaData(meta, "", 0);

  // Try the kodak hidden IFD for WB
  if (mRootIFD->hasEntryRecursive(TiffTag::KODAK_IFD2)) {
    const TiffEntry* ifdoffset =
        mRootIFD->getEntryRecursive(TiffTag::KODAK_IFD2);
    try {
      NORangesSet<Buffer> ifds;

      TiffRootIFD kodakifd(nullptr, &ifds, ifdoffset->getRootIfdData(),
                           ifdoffset->getU32());

      if (kodakifd.hasEntryRecursive(TiffTag::KODAK_KDC_WB)) {
        const TiffEntry* wb = kodakifd.getEntryRecursive(TiffTag::KODAK_KDC_WB);
        if (wb->count == 3) {
          mRaw->metadata.wbCoeffs[0] = wb->getFloat(0);
          mRaw->metadata.wbCoeffs[1] = wb->getFloat(1);
          mRaw->metadata.wbCoeffs[2] = wb->getFloat(2);
        }
      }
    } catch (const TiffParserException& e) {
      mRaw->setError(e.what());
    }
  }

  // Use the normal WB if available
  if (mRootIFD->hasEntryRecursive(TiffTag::KODAKWB)) {
    const TiffEntry* wb = mRootIFD->getEntryRecursive(TiffTag::KODAKWB);
    if (wb->count == 734 || wb->count == 1502) {
      mRaw->metadata.wbCoeffs[0] =
          static_cast<float>(((static_cast<uint16_t>(wb->getByte(148))) << 8) |
                             wb->getByte(149)) /
          256.0F;
      mRaw->metadata.wbCoeffs[1] = 1.0F;
      mRaw->metadata.wbCoeffs[2] =
          static_cast<float>(((static_cast<uint16_t>(wb->getByte(150))) << 8) |
                             wb->getByte(151)) /
          256.0F;
    }
  }
}

} // namespace rawspeed
