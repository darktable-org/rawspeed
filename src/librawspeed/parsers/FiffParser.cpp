/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2017 Axel Waggershauser
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

#include "parsers/FiffParser.h"
#include "common/Common.h"               // for make_unique, uint32, uchar8
#include "decoders/RawDecoder.h"         // for RawDecoder
#include "io/Buffer.h"                   // for Buffer
#include "io/ByteStream.h"               // for ByteStream
#include "io/Endianness.h"               // for getU32BE, getHostEndianness
#include "parsers/FiffParserException.h" // for ThrowFPE
#include "parsers/RawParser.h"           // for RawParser
#include "parsers/TiffParser.h" // for TiffParser::parse, TiffParser::makeDecoder
#include "parsers/TiffParserException.h" // for TiffParserException
#include "tiff/TiffEntry.h"              // for TiffEntry, TiffDataType::TI...
#include "tiff/TiffIFD.h"                // for TiffIFD, TiffRootIFD, TiffI...
#include "tiff/TiffTag.h"                // for TiffTag, TiffTag::FUJIOLDWB
#include <algorithm>                     // for move
#include <limits>                        // for numeric_limits
#include <memory>                        // for default_delete, unique_ptr

using std::numeric_limits;

namespace rawspeed {

FiffParser::FiffParser(const Buffer* inputData) : RawParser(inputData) {}

void FiffParser::parseData() {
  const uchar8* data = mInput->getData(0, 104);

  uint32 first_ifd = getU32BE(data + 0x54);
  if (first_ifd >= numeric_limits<uint32>::max() - 12)
    ThrowFPE("Not Fiff. First IFD too far away");

  first_ifd += 12;

  uint32 second_ifd = getU32BE(data + 0x64);
  uint32 third_ifd = getU32BE(data + 0x5C);

  rootIFD = TiffParser::parse(mInput->getSubView(first_ifd));
  TiffIFDOwner subIFD = make_unique<TiffIFD>(rootIFD.get());

  if (mInput->isValid(second_ifd)) {
    // RAW Tiff on newer models, pointer to raw data on older models
    // -> so we try parsing as Tiff first and add it as data if parsing fails
    try {
      rootIFD->add(TiffParser::parse(mInput->getSubView(second_ifd)));
    } catch (TiffParserException&) {
      // the offset will be interpreted relative to the rootIFD where this
      // subIFD gets inserted

      if (second_ifd <= first_ifd)
        ThrowFPE("Fiff is corrupted: second IFD is not after the first IFD");

      uint32 rawOffset = second_ifd - first_ifd;
      subIFD->add(
          make_unique<TiffEntry>(subIFD.get(), FUJI_STRIPOFFSETS, TIFF_OFFSET,
                                 1, ByteStream::createCopy(&rawOffset, 4)));
      uint32 max_size = mInput->getSize() - second_ifd;
      subIFD->add(make_unique<TiffEntry>(subIFD.get(), FUJI_STRIPBYTECOUNTS,
                                         TIFF_LONG, 1,
                                         ByteStream::createCopy(&max_size, 4)));
    }
  }

  if (mInput->isValid(third_ifd)) {
    // RAW information IFD on older

    // This Fuji directory structure is similar to a Tiff IFD but with two
    // differences:
    //   a) no type info and b) data is always stored in place.
    // 4b: # of entries, for each entry: 2b tag, 2b len, xb data
    ByteStream bytes(mInput, third_ifd, getHostEndianness() == Endianness::big);
    uint32 entries = bytes.getU32();

    if (entries > 255)
      ThrowFPE("Too many entries");

    for (uint32 i = 0; i < entries; i++) {
      ushort16 tag = bytes.getU16();
      ushort16 length = bytes.getU16();
      TiffDataType type = TIFF_UNDEFINED;

      if (tag == IMAGEWIDTH || tag == FUJIOLDWB) // also 0x121?
        type = TIFF_SHORT;

      uint32 count = type == TIFF_SHORT ? length / 2 : length;
      subIFD->add(make_unique<TiffEntry>(
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

  try {
    return TiffParser::makeDecoder(move(rootIFD), *mInput);
  } catch (TiffParserException&) {
    ThrowFPE("No decoder found. Sorry.");
  }
}

} // namespace rawspeed
