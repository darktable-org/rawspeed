/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2017 Axel Waggershauser
    Copyright (C) 2017-2018 Roman Lebedev

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

#include "parsers/FiffParser.h"
#include "decoders/RafDecoder.h"
#include "io/Buffer.h"
#include "io/ByteStream.h"
#include "io/Endianness.h"
#include "parsers/FiffParserException.h"
#include "parsers/RawParser.h"
#include "parsers/TiffParser.h"
#include "parsers/TiffParserException.h"
#include "tiff/TiffEntry.h"
#include "tiff/TiffIFD.h"
#include "tiff/TiffTag.h"
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>

using std::numeric_limits;

namespace rawspeed {

FiffParser::FiffParser(Buffer inputData) : RawParser(inputData) {}

void FiffParser::parseData() {
  ByteStream bs(DataBuffer(mInput, Endianness::big));
  bs.skipBytes(0x54);

  uint32_t first_ifd = bs.getU32();
  if (first_ifd >= numeric_limits<uint32_t>::max() - 12)
    ThrowFPE("Not Fiff. First IFD too far away");

  first_ifd += 12;

  bs.skipBytes(4);
  const uint32_t third_ifd = bs.getU32();
  bs.skipBytes(4);
  const uint32_t second_ifd = bs.getU32();

  rootIFD = TiffParser::parse(nullptr, mInput.getSubView(first_ifd));
  auto subIFD = std::make_unique<TiffIFD>(rootIFD.get());

  if (mInput.isValid(second_ifd)) {
    // RAW Tiff on newer models, pointer to raw data on older models
    // -> so we try parsing as Tiff first and add it as data if parsing fails
    try {
      rootIFD->add(
          TiffParser::parse(rootIFD.get(), mInput.getSubView(second_ifd)));
    } catch (const TiffParserException&) {
      // the offset will be interpreted relative to the rootIFD where this
      // subIFD gets inserted

      if (second_ifd <= first_ifd)
        ThrowFPE("Fiff is corrupted: second IFD is not after the first IFD");

      uint32_t rawOffset = second_ifd - first_ifd;
      subIFD->add(std::make_unique<TiffEntryWithData>(
          subIFD.get(), TiffTag::FUJI_STRIPOFFSETS, TiffDataType::OFFSET, 1,
          Buffer(reinterpret_cast<const uint8_t*>(&rawOffset),
                 sizeof(rawOffset))));
      uint32_t max_size = mInput.getSize() - second_ifd;
      subIFD->add(std::make_unique<TiffEntryWithData>(
          subIFD.get(), TiffTag::FUJI_STRIPBYTECOUNTS, TiffDataType::LONG, 1,
          Buffer(reinterpret_cast<const uint8_t*>(&max_size),
                 sizeof(rawOffset))));
    }
  }

  if (mInput.isValid(third_ifd)) {
    // RAW information IFD on older

    // This Fuji directory structure is similar to a Tiff IFD but with two
    // differences:
    //   a) no type info and b) data is always stored in place.
    // 4b: # of entries, for each entry: 2b tag, 2b len, xb data
    ByteStream bytes(DataBuffer(mInput.getSubView(third_ifd), Endianness::big));
    uint32_t entries = bytes.getU32();

    if (entries > 255)
      ThrowFPE("Too many entries");

    for (uint32_t i = 0; i < entries; i++) {
      auto tag = static_cast<TiffTag>(bytes.getU16());
      uint16_t length = bytes.getU16();

      TiffDataType type;
      switch (tag) {
        using enum TiffTag;
      case TiffTag::FUJI_RAWIMAGEFULLSIZE:
      case TiffTag::FUJI_RAWIMAGECROPTOPLEFT:
      case TiffTag::FUJI_RAWIMAGECROPPEDSIZE:
      case TiffTag::FUJIOLDWB:
        // also 0x121?
        type = TiffDataType::SHORT;
        break;
      default:
        type = TiffDataType::UNDEFINED;
        break;
      }

      uint32_t count = type == TiffDataType::SHORT ? length / 2 : length;
      subIFD->add(std::make_unique<TiffEntry>(
          subIFD.get(), tag, type, count,
          bytes.getSubStream(bytes.getPosition(), length)));

      bytes.skipBytes(length);
    }
  }

  rootIFD->add(std::move(subIFD));
}

std::unique_ptr<RawDecoder> FiffParser::getDecoder(const CameraMetaData* meta) {
  if (!rootIFD)
    parseData();

  // WARNING: do *NOT* fallback to ordinary TIFF parser here!
  // All the FIFF raws are '.RAF' (Fujifilm). Do use RafDecoder directly.

  try {
    if (!RafDecoder::isAppropriateDecoder(rootIFD.get(), mInput))
      ThrowFPE("Not a FUJIFILM RAF FIFF.");

    return std::make_unique<RafDecoder>(std::move(rootIFD), mInput);
  } catch (const TiffParserException&) {
    ThrowFPE("No decoder found. Sorry.");
  }
}

} // namespace rawspeed
