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
#include "decoders/RafDecoder.h"         // for RafDecoder
#include "decoders/RawDecoder.h"         // for RawDecoder
#include "io/Buffer.h"                   // for Buffer, DataBuffer
#include "io/ByteStream.h"               // for ByteStream
#include "io/Endianness.h"               // for Endianness, Endianness::big
#include "parsers/FiffParserException.h" // for ThrowFPE
#include "parsers/RawParser.h"           // for RawParser
#include "parsers/TiffParser.h"          // for TiffParser
#include "parsers/TiffParserException.h" // for TiffParserException
#include "tiff/TiffEntry.h"              // for TiffEntry, TIFF_SHORT, TIFF...
#include "tiff/TiffIFD.h"                // for TiffIFD, TiffRootIFDOwner
#include "tiff/TiffTag.h"                // for FUJIOLDWB, FUJI_STRIPBYTECO...
#include <cstdint>                       // for uint32_t, uint16_t
#include <limits>                        // for numeric_limits
#include <memory>                        // for make_unique, unique_ptr
#include <utility>                       // for move

using std::numeric_limits;

namespace rawspeed {

FiffParser::FiffParser(const Buffer* inputData) : RawParser(inputData) {}

void FiffParser::parseData() {
  ByteStream bs(DataBuffer(*mInput, Endianness::big));
  bs.skipBytes(0x54);

  uint32_t first_ifd = bs.getU32();
  if (first_ifd >= numeric_limits<uint32_t>::max() - 12)
    ThrowFPE("Not Fiff. First IFD too far away");

  first_ifd += 12;

  bs.skipBytes(4);
  const uint32_t third_ifd = bs.getU32();
  bs.skipBytes(4);
  const uint32_t second_ifd = bs.getU32();

  rootIFD = TiffParser::parse(nullptr, mInput->getSubView(first_ifd));
  TiffIFDOwner subIFD = std::make_unique<TiffIFD>(rootIFD.get());

  if (mInput->isValid(second_ifd)) {
    // RAW Tiff on newer models, pointer to raw data on older models
    // -> so we try parsing as Tiff first and add it as data if parsing fails
    try {
      rootIFD->add(
          TiffParser::parse(rootIFD.get(), mInput->getSubView(second_ifd)));
    } catch (TiffParserException&) {
      // the offset will be interpreted relative to the rootIFD where this
      // subIFD gets inserted

      if (second_ifd <= first_ifd)
        ThrowFPE("Fiff is corrupted: second IFD is not after the first IFD");

      uint32_t rawOffset = second_ifd - first_ifd;
      subIFD->add(std::make_unique<TiffEntry>(
          subIFD.get(), FUJI_STRIPOFFSETS, TIFF_OFFSET, 1,
          ByteStream::createCopy(&rawOffset, 4)));
      uint32_t max_size = mInput->getSize() - second_ifd;
      subIFD->add(std::make_unique<TiffEntry>(
          subIFD.get(), FUJI_STRIPBYTECOUNTS, TIFF_LONG, 1,
          ByteStream::createCopy(&max_size, 4)));
    }
  }

  if (mInput->isValid(third_ifd)) {
    // RAW information IFD on older

    // This Fuji directory structure is similar to a Tiff IFD but with two
    // differences:
    //   a) no type info and b) data is always stored in place.
    // 4b: # of entries, for each entry: 2b tag, 2b len, xb data
    ByteStream bytes(
        DataBuffer(mInput->getSubView(third_ifd), Endianness::big));
    uint32_t entries = bytes.getU32();

    if (entries > 255)
      ThrowFPE("Too many entries");

    for (uint32_t i = 0; i < entries; i++) {
      uint16_t tag = bytes.getU16();
      uint16_t length = bytes.getU16();
      TiffDataType type = TIFF_UNDEFINED;

      if (tag == IMAGEWIDTH || tag == FUJIOLDWB) // also 0x121?
        type = TIFF_SHORT;

      uint32_t count = type == TIFF_SHORT ? length / 2 : length;
      subIFD->add(std::make_unique<TiffEntry>(
          subIFD.get(), static_cast<TiffTag>(tag), type, count,
          bytes.getSubStream(bytes.getPosition(), length)));

      bytes.skipBytes(length);
    }
  }

  rootIFD->add(move(subIFD));
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
  } catch (TiffParserException&) {
    ThrowFPE("No decoder found. Sorry.");
  }
}

} // namespace rawspeed
