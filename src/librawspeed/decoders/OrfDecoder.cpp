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
#include "adt/Array1DRef.h"
#include "adt/Array2DRef.h"
#include "adt/Casts.h"
#include "adt/NORangesSet.h"
#include "adt/Point.h"
#include "bitstreams/BitStreamerMSB.h"
#include "bitstreams/BitStreams.h"
#include "common/Common.h"
#include "common/RawImage.h"
#include "decoders/RawDecoderException.h"
#include "decompressors/OlympusDecompressor.h"
#include "decompressors/UncompressedDecompressor.h"
#include "io/Buffer.h"
#include "io/ByteStream.h"
#include "io/Endianness.h"
#include "metadata/ColorFilterArray.h"
#include "tiff/TiffEntry.h"
#include "tiff/TiffIFD.h"
#include "tiff/TiffTag.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <memory>
#include <string>

namespace rawspeed {

class CameraMetaData;

bool OrfDecoder::isAppropriateDecoder(const TiffRootIFD* rootIFD,
                                      [[maybe_unused]] Buffer file) {
  const auto id = rootIFD->getID();
  const std::string& make = id.make;

  // FIXME: magic

  return make == "OLYMPUS IMAGING CORP." || make == "OLYMPUS CORPORATION" ||
         make == "OLYMPUS OPTICAL CO.,LTD" || make == "OM Digital Solutions";
}

ByteStream OrfDecoder::handleSlices() const {
  const auto* raw = mRootIFD->getIFDWithTag(TiffTag::STRIPOFFSETS);

  const TiffEntry* offsets = raw->getEntry(TiffTag::STRIPOFFSETS);
  const TiffEntry* counts = raw->getEntry(TiffTag::STRIPBYTECOUNTS);

  if (counts->count != offsets->count) {
    ThrowRDE(
        "Byte count number does not match strip size: count:%u, strips:%u ",
        counts->count, offsets->count);
  }

  const uint32_t off = offsets->getU32(0);
  uint32_t size = counts->getU32(0);
  auto end = [&off, &size]() { return off + size; };

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

  if (int compression = raw->getEntry(TiffTag::COMPRESSION)->getU32();
      1 != compression)
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

  if (mRootIFD->hasEntryRecursive(TiffTag::OLYMPUSIMAGEPROCESSING)) {
    // Newer cameras process the Image Processing SubIFD in the makernote
    const TiffEntry* img_entry =
        mRootIFD->getEntryRecursive(TiffTag::OLYMPUSIMAGEPROCESSING);
    // get makernote ifd with containing Buffer
    NORangesSet<Buffer> ifds;

    TiffRootIFD image_processing(nullptr, &ifds, img_entry->getRootIfdData(),
                                 img_entry->getU32());

    if (image_processing.hasEntry(static_cast<TiffTag>(0x0611))) {
      const TiffEntry* validBits =
          image_processing.getEntry(static_cast<TiffTag>(0x0611));
      if (validBits->getU16() != 12)
        ThrowRDE("Only 12-bit images are supported currently.");
    }
  }

  OlympusDecompressor o(mRaw);
  mRaw->createData();
  o.decompress(input);

  return mRaw;
}

void OrfDecoder::decodeUncompressedInterleaved(ByteStream s, uint32_t w,
                                               uint32_t h,
                                               uint32_t size) const {
  int inputPitchBits = 12 * w;
  assert(inputPitchBits % 8 == 0);

  int inputPitchBytes = inputPitchBits / 8;

  const auto numEvenLines = implicit_cast<int>(roundUpDivisionSafe(h, 2));
  const auto evenLinesInput = s.getStream(numEvenLines, inputPitchBytes)
                                  .peekRemainingBuffer()
                                  .getAsArray1DRef();

  const auto oddLinesInputBegin =
      implicit_cast<int>(roundUp(evenLinesInput.size(), 1U << 11U));
  assert(oddLinesInputBegin >= evenLinesInput.size());
  int padding = oddLinesInputBegin - evenLinesInput.size();
  assert(padding >= 0);
  s.skipBytes(padding);

  const int numOddLines = h - numEvenLines;
  const auto oddLinesInput = s.getStream(numOddLines, inputPitchBytes)
                                 .peekRemainingBuffer()
                                 .getAsArray1DRef();

  // By now we know we have enough input to produce the image.
  mRaw->createData();

  const Array2DRef<uint16_t> out(mRaw->getU16DataAsUncroppedArray2DRef());
  {
    BitStreamerMSB bs(evenLinesInput);
    for (int i = 0; i != numEvenLines; ++i) {
      for (unsigned col = 0; col != w; ++col) {
        int row = 2 * i;
        out(row, col) = implicit_cast<uint16_t>(bs.getBits(12));
      }
    }
  }
  {
    BitStreamerMSB bs(oddLinesInput);
    for (int i = 0; i != numOddLines; ++i) {
      for (unsigned col = 0; col != w; ++col) {
        int row = 1 + 2 * i;
        out(row, col) = implicit_cast<uint16_t>(bs.getBits(12));
      }
    }
  }
}

bool OrfDecoder::decodeUncompressed(ByteStream s, uint32_t w, uint32_t h,
                                    uint32_t size) const {
  // FIXME: most of this logic should be in UncompressedDecompressor,
  // one way or another.

  if (size == h * ((w * 12 / 8) + ((w + 2) / 10))) {
    // 12-bit  packed 'with control' raw
    UncompressedDecompressor u(s, mRaw, iRectangle2D({0, 0}, iPoint2D(w, h)),
                               (12 * w / 8) + ((w + 2) / 10), 12,
                               BitOrder::LSB);
    mRaw->createData();
    u.decode12BitRawWithControl<Endianness::little>();
    return true;
  }

  iPoint2D dimensions(w, h);
  iPoint2D pos(0, 0);
  if (size == w * h * 12 / 8) { // We're in a 12-bit packed raw
    UncompressedDecompressor u(s, mRaw, iRectangle2D(pos, dimensions),
                               w * 12 / 8, 12, BitOrder::MSB32);
    mRaw->createData();
    u.readUncompressedRaw();
    return true;
  }

  if (size == w * h * 2) { // We're in an unpacked raw
    // FIXME: seems fishy
    if (s.getByteOrder() == getHostEndianness()) {
      UncompressedDecompressor u(s, mRaw, iRectangle2D({0, 0}, iPoint2D(w, h)),
                                 16 * w / 8, 16, BitOrder::LSB);
      mRaw->createData();
      u.decode12BitRawUnpackedLeftAligned<Endianness::little>();
    } else {
      UncompressedDecompressor u(s, mRaw, iRectangle2D({0, 0}, iPoint2D(w, h)),
                                 16 * w / 8, 16, BitOrder::MSB);
      mRaw->createData();
      u.decode12BitRawUnpackedLeftAligned<Endianness::big>();
    }
    return true;
  }

  if (size > w * h * 3 / 2) {
    // We're in one of those weird interlaced packed raws
    decodeUncompressedInterleaved(s, w, h, size);
    return true;
  }

  // Does not appear to be uncomporessed. Maybe it's compressed?
  return false;
}

void OrfDecoder::parseCFA() const {
  if (!mRootIFD->hasEntryRecursive(TiffTag::EXIFCFAPATTERN))
    ThrowRDE("No EXIFCFAPATTERN entry found!");

  const TiffEntry* CFA = mRootIFD->getEntryRecursive(TiffTag::EXIFCFAPATTERN);
  if (CFA->type != TiffDataType::UNDEFINED || CFA->count != 8) {
    ThrowRDE("Bad EXIFCFAPATTERN entry (type %u, count %u).",
             static_cast<unsigned>(CFA->type), CFA->count);
  }

  iPoint2D cfaSize(CFA->getU16(0), CFA->getU16(1));
  if (cfaSize != iPoint2D{2, 2})
    ThrowRDE("Bad CFA size: (%i, %i)", cfaSize.x, cfaSize.y);

  mRaw->cfa.setSize(cfaSize);

  auto int2enum = [](uint8_t i) {
    switch (i) {
      using enum CFAColor;
    case 0:
      return RED;
    case 1:
      return GREEN;
    case 2:
      return BLUE;
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
  mRaw->whitePoint = (1U << 12) - 1;

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
    const TiffEntry* img_entry =
        mRootIFD->getEntryRecursive(TiffTag::OLYMPUSIMAGEPROCESSING);
    // get makernote ifd with containing Buffer
    NORangesSet<Buffer> ifds;

    TiffRootIFD image_processing(nullptr, &ifds, img_entry->getRootIfdData(),
                                 img_entry->getU32());

    // Get the WB
    if (image_processing.hasEntry(static_cast<TiffTag>(0x0100))) {
      const TiffEntry* wb =
          image_processing.getEntry(static_cast<TiffTag>(0x0100));
      if (wb->count == 2 || wb->count == 4) {
        mRaw->metadata.wbCoeffs[0] = wb->getFloat(0);
        mRaw->metadata.wbCoeffs[1] = 256.0F;
        mRaw->metadata.wbCoeffs[2] = wb->getFloat(1);
      }
    }

    // Get the black levels
    if (image_processing.hasEntry(static_cast<TiffTag>(0x0600))) {
      const TiffEntry* blackEntry =
          image_processing.getEntry(static_cast<TiffTag>(0x0600));
      // Order is assumed to be RGGB
      if (blackEntry->count == 4) {
        mRaw->blackLevelSeparate =
            Array2DRef(mRaw->blackLevelSeparateStorage.data(), 2, 2);
        auto blackLevelSeparate1D =
            *mRaw->blackLevelSeparate->getAsArray1DRef();
        for (int i = 0; i < 4; i++) {
          auto c = mRaw->cfa.getColorAt(i & 1, i >> 1);
          int j;
          switch (c) {
            using enum CFAColor;
          case RED:
            j = 0;
            break;
          case GREEN:
            j = i < 2 ? 1 : 2;
            break;
          case BLUE:
            j = 3;
            break;
          default:
            ThrowRDE("Unexpected CFA color: %u", static_cast<unsigned>(c));
          }

          blackLevelSeparate1D(i) = blackEntry->getU16(j);
        }
        // Adjust whitelevel based on the read black (we assume the dynamic
        // range is the same)
        mRaw->whitePoint =
            *mRaw->whitePoint - (mRaw->blackLevel - blackLevelSeparate1D(0));
      }
    }
  }
}

} // namespace rawspeed
