/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2014-2015 Pedro Côrte-Real
    Copyright (C) 2017 Roman Lebedev

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

#include "decoders/OrfDecoder.h"
#include "common/Common.h"                          // for uint32_t, uint8_t
#include "common/NORangesSet.h"                     // for set
#include "common/Point.h"                           // for iPoint2D
#include "decoders/RawDecoderException.h"           // for ThrowRDE
#include "decompressors/OlympusDecompressor.h"      // for OlympusDecompressor
#include "decompressors/UncompressedDecompressor.h" // for UncompressedDeco...
#include "io/Buffer.h"                              // for Buffer
#include "io/ByteStream.h"                          // for ByteStream
#include "io/Endianness.h"                          // for Endianness, getH...
#include "metadata/ColorFilterArray.h"              // for ColorFilterArray
#include "tiff/TiffEntry.h"                         // for TiffEntry, TIFF_...
#include "tiff/TiffIFD.h"                           // for TiffRootIFD, Tif...
#include "tiff/TiffTag.h"                           // for STRIPOFFSETS
#include <array>                                    // for array
#include <memory>                                   // for unique_ptr
#include <string>                                   // for operator==, string

namespace rawspeed {

class CameraMetaData;

bool OrfDecoder::isAppropriateDecoder(const TiffRootIFD* rootIFD,
                                      const Buffer& file) {
  const auto id = rootIFD->getID();
  const std::string& make = id.make;

  // FIXME: magic

  return make == "OLYMPUS IMAGING CORP." || make == "OLYMPUS CORPORATION" ||
         make == "OLYMPUS OPTICAL CO.,LTD";
}

ByteStream OrfDecoder::handleSlices() const {
  const auto* raw = mRootIFD->getIFDWithTag(TiffTag::STRIPOFFSETS);

  TiffEntry* offsets = raw->getEntry(TiffTag::STRIPOFFSETS);
  TiffEntry* counts = raw->getEntry(TiffTag::STRIPBYTECOUNTS);

  if (counts->count != offsets->count) {
    ThrowRDE(
        "Byte count number does not match strip size: count:%u, strips:%u ",
        counts->count, offsets->count);
  }

  const uint32_t off = offsets->getU32(0);
  uint32_t size = counts->getU32(0);
  auto end = [&off, &size]() -> uint32_t { return off + size; };

  for (uint32_t i = 0; i < counts->count; i++) {
    const auto offset = offsets->getU32(i);
    const auto count = counts->getU32(i);
    if (!mFile.isValid(offset, count))
      ThrowRDE("Truncated file");

    if (count < 1)
      ThrowRDE("Empty slice");

    if (i == 0)
      continue;

    if (offset < end())
      ThrowRDE("Slices overlap");

    // Now, everything would be great, but some uncompressed raws
    // (packed_with_control i believe) have "padding" between at least
    // the first two slices, and we need to account for it.
    const uint32_t padding = offset - end();

    size += padding;
    size += count;
  }

  ByteStream input(offsets->getRootIfdData());
  input.setPosition(off);

  return input.getStream(size);
}

RawImage OrfDecoder::decodeRawInternal() {
  const auto* raw = mRootIFD->getIFDWithTag(TiffTag::STRIPOFFSETS);

  int compression = raw->getEntry(TiffTag::COMPRESSION)->getU32();
  if (1 != compression)
    ThrowRDE("Unsupported compression");

  uint32_t width = raw->getEntry(TiffTag::IMAGEWIDTH)->getU32();
  uint32_t height = raw->getEntry(TiffTag::IMAGELENGTH)->getU32();

  if (!width || !height || width % 2 != 0 || width > 10400 || height > 7796)
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", width, height);

  mRaw->dim = iPoint2D(width, height);

  ByteStream input(handleSlices());

  if (decodeUncompressed(input, width, height, input.getSize()))
    return mRaw;

  if (raw->getEntry(TiffTag::STRIPOFFSETS)->count != 1)
    ThrowRDE("%u stripes, and not uncompressed. Unsupported.",
             raw->getEntry(TiffTag::STRIPOFFSETS)->count);

  OlympusDecompressor o(mRaw);
  mRaw->createData();
  o.decompress(std::move(input));

  return mRaw;
}

bool OrfDecoder::decodeUncompressed(const ByteStream& s, uint32_t w, uint32_t h,
                                    uint32_t size) const {
  UncompressedDecompressor u(s, mRaw);
  // FIXME: most of this logic should be in UncompressedDecompressor,
  // one way or another.

  if (size == h * ((w * 12 / 8) + ((w + 2) / 10))) {
    // 12-bit  packed 'with control' raw
    mRaw->createData();
    u.decode12BitRaw<Endianness::little, false, true>(w, h);
    return true;
  }

  if (size == w * h * 12 / 8) { // We're in a 12-bit packed raw
    iPoint2D dimensions(w, h);
    iPoint2D pos(0, 0);
    mRaw->createData();
    u.readUncompressedRaw(dimensions, pos, w * 12 / 8, 12, BitOrder::MSB32);
    return true;
  }

  if (size == w * h * 2) { // We're in an unpacked raw
    mRaw->createData();
    // FIXME: seems fishy
    if (s.getByteOrder() == getHostEndianness())
      u.decodeRawUnpacked<12, Endianness::little>(w, h);
    else
      u.decode12BitRawUnpackedLeftAligned<Endianness::big>(w, h);
    return true;
  }

  if (size >
      w * h * 3 / 2) { // We're in one of those weird interlaced packed raws
    mRaw->createData();
    u.decode12BitRaw<Endianness::big, true>(w, h);
    return true;
  }

  // Does not appear to be uncomporessed. Maybe it's compressed?
  return false;
}

void OrfDecoder::parseCFA() const {
  if (!mRootIFD->hasEntryRecursive(TiffTag::EXIFCFAPATTERN))
    ThrowRDE("No EXIFCFAPATTERN entry found!");

  TiffEntry* CFA = mRootIFD->getEntryRecursive(TiffTag::EXIFCFAPATTERN);
  if (CFA->type != TiffDataType::UNDEFINED || CFA->count != 8) {
    ThrowRDE("Bad EXIFCFAPATTERN entry (type %u, count %u).",
             static_cast<unsigned>(CFA->type), CFA->count);
  }

  iPoint2D cfaSize(CFA->getU16(0), CFA->getU16(1));
  if (cfaSize != iPoint2D{2, 2})
    ThrowRDE("Bad CFA size: (%i, %i)", cfaSize.x, cfaSize.y);

  mRaw->cfa.setSize(cfaSize);

  auto int2enum = [](uint8_t i) -> CFAColor {
    switch (i) {
    case 0:
      return CFAColor::RED;
    case 1:
      return CFAColor::GREEN;
    case 2:
      return CFAColor::BLUE;
    default:
      ThrowRDE("Unexpected CFA color: %u", i);
    }
  };

  for (int y = 0; y < cfaSize.y; y++) {
    for (int x = 0; x < cfaSize.x; x++) {
      uint8_t c1 = CFA->getByte(4 + x + y * cfaSize.x);
      CFAColor c2 = int2enum(c1);
      mRaw->cfa.setColorAt(iPoint2D(x, y), c2);
    }
  }
}

void OrfDecoder::decodeMetaDataInternal(const CameraMetaData* meta) {
  int iso = 0;

  if (mRootIFD->hasEntryRecursive(TiffTag::ISOSPEEDRATINGS))
    iso = mRootIFD->getEntryRecursive(TiffTag::ISOSPEEDRATINGS)->getU32();

  parseCFA();

  setMetaData(meta, "", iso);

  if (mRootIFD->hasEntryRecursive(TiffTag::OLYMPUSREDMULTIPLIER) &&
      mRootIFD->hasEntryRecursive(TiffTag::OLYMPUSBLUEMULTIPLIER)) {
    mRaw->metadata.wbCoeffs[0] = static_cast<float>(
        mRootIFD->getEntryRecursive(TiffTag::OLYMPUSREDMULTIPLIER)->getU16());
    mRaw->metadata.wbCoeffs[1] = 256.0F;
    mRaw->metadata.wbCoeffs[2] = static_cast<float>(
        mRootIFD->getEntryRecursive(TiffTag::OLYMPUSBLUEMULTIPLIER)->getU16());
  } else if (mRootIFD->hasEntryRecursive(TiffTag::OLYMPUSIMAGEPROCESSING)) {
    // Newer cameras process the Image Processing SubIFD in the makernote
    TiffEntry* img_entry =
        mRootIFD->getEntryRecursive(TiffTag::OLYMPUSIMAGEPROCESSING);
    // get makernote ifd with containing Buffer
    NORangesSet<Buffer> ifds;

    TiffRootIFD image_processing(nullptr, &ifds, img_entry->getRootIfdData(),
                                 img_entry->getU32());

    // Get the WB
    if (image_processing.hasEntry(static_cast<TiffTag>(0x0100))) {
      TiffEntry* wb = image_processing.getEntry(static_cast<TiffTag>(0x0100));
      if (wb->count == 2 || wb->count == 4) {
        mRaw->metadata.wbCoeffs[0] = wb->getFloat(0);
        mRaw->metadata.wbCoeffs[1] = 256.0F;
        mRaw->metadata.wbCoeffs[2] = wb->getFloat(1);
      }
    }

    // Get the black levels
    if (image_processing.hasEntry(static_cast<TiffTag>(0x0600))) {
      TiffEntry* blackEntry =
          image_processing.getEntry(static_cast<TiffTag>(0x0600));
      // Order is assumed to be RGGB
      if (blackEntry->count == 4) {
        for (int i = 0; i < 4; i++) {
          auto c = mRaw->cfa.getColorAt(i & 1, i >> 1);
          int j;
          switch (c) {
          case CFAColor::RED:
            j = 0;
            break;
          case CFAColor::GREEN:
            j = i < 2 ? 1 : 2;
            break;
          case CFAColor::BLUE:
            j = 3;
            break;
          default:
            ThrowRDE("Unexpected CFA color: %u", static_cast<unsigned>(c));
          }

          mRaw->blackLevelSeparate[i] = blackEntry->getU16(j);
        }
        // Adjust whitelevel based on the read black (we assume the dynamic
        // range is the same)
        mRaw->whitePoint -= (mRaw->blackLevel - mRaw->blackLevelSeparate[0]);
      }
    }
  }
}

} // namespace rawspeed
