#include "StdAfx.h"
#include "RawParser.h"
#include "TiffParserException.h"
#include "TiffParser.h"
#include "CiffParserException.h"
#include "CiffParser.h"
#include "X3fParser.h"
#include "AriDecoder.h"
#include "MrwDecoder.h"
#include "NakedDecoder.h"

/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2017 Axel Waggershauser

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

    http://www.klauspost.com
*/

namespace RawSpeed {


RawParser::RawParser(FileMap* inputData): mInput(inputData) {
}


RawParser::~RawParser(void) {
}

RawDecoder* RawParser::getDecoder(CameraMetaData* meta) {
  // We need some data.
  // For now it is 104 bytes for RAF/FUJIFIM images.
  if (mInput->getSize() <=  104)
    ThrowRDE("File too small");

  const unsigned char* data = mInput->getData(0, 104);

  // MRW images are easy to check for, let's try that first
  if (MrwDecoder::isMRW(mInput)) {
    try {
      return new MrwDecoder(mInput);
    } catch (RawDecoderException) {
    }
  }

  if (0 == memcmp(&data[0], "ARRI\x12\x34\x56\x78", 8)) {
    try {
      return new AriDecoder(mInput);
    } catch (RawDecoderException) {
    }
  }

  // FUJI has pointers to IFD's at fixed byte offsets
  // So if camera is FUJI, we cannot use ordinary TIFF parser
  if (0 == memcmp(data, "FUJIFILMCCD-RAW ", 16)) {
    //TODO: fix byte order and move to separate decoder/parser
    // First IFD typically JPEG and EXIF with a TIFF starting at offset 12
    uint32 first_ifd = get4BE(data, 0x54) + 12;
    uint32 second_ifd = get4BE(data, 0x64);
    uint32 third_ifd = get4BE(data, 0x5C);

    // Open the IFDs and merge them
    try {
      TiffRootIFDOwner rootIFD = parseTiff(mInput->getSubView(first_ifd));
      TiffIFDOwner subIFD = make_unique<TiffIFD>();

      if (mInput->isValid(second_ifd)) {
        // RAW Tiff on newer models, pointer to raw data on older models
        // -> so we try parsing as Tiff first and add it as data if parsing fails
        try {
          rootIFD->add(parseTiff(mInput->getSubView(second_ifd)));
        } catch (TiffParserException) {
          // the offset will be interpreted relative to the rootIFD where this subIFD gets inserted
          uint32 rawOffset = second_ifd - first_ifd;
          subIFD->add(make_unique<TiffEntry>(FUJI_STRIPOFFSETS, TIFF_OFFSET, 1, ByteStream::createCopy(&rawOffset, 4)));
          uint32 max_size = mInput->getSize() - second_ifd;
          subIFD->add(make_unique<TiffEntry>(FUJI_STRIPBYTECOUNTS, TIFF_LONG, 1, ByteStream::createCopy(&max_size, 4)));
        }
      }

      if (mInput->isValid(third_ifd)) {
        // RAW information IFD on older

        // This Fuji directory structure is similar to a Tiff IFD but with two differences:
        //   a) no type info and b) data is always stored in place.
        // 4b: # of entries, for each entry: 2b tag, 2b len, xb data
        ByteStream bytes(mInput, third_ifd, getHostEndianness() == big);
        uint32 entries = bytes.getUInt();

        if (entries > 255)
          ThrowTPE("ParseFuji: Too many entries");

        for (uint32 i = 0; i < entries; i++) {
          ushort16 tag = bytes.getShort();
          ushort16 length = bytes.getShort();
          TiffDataType type = TIFF_UNDEFINED;

          if (tag == IMAGEWIDTH || tag == FUJIOLDWB) // also 0x121?
            type = TIFF_SHORT;

          uint32 count = type == TIFF_SHORT ? length/2 : length;
          subIFD->add(make_unique<TiffEntry>((TiffTag)tag, type, count, bytes.getSubStream(bytes.getPosition(), length)));

          bytes.skipBytes(length);
        }
      }

      rootIFD->add(move(subIFD));

      return makeDecoder(move(rootIFD), *mInput);
    } catch (TiffParserException) {}
    ThrowRDE("No decoder found. Sorry.");
  }

  // Ordinary TIFF images
  try {
    return makeDecoder(parseTiff(*mInput), *mInput);
  } catch (TiffParserException) {}

  try {
    X3fParser parser(mInput);
    return parser.getDecoder();
  } catch (RawDecoderException) {
  }

  // CIFF images
  try {
    CiffParser p(mInput);
    p.parseData();
    return p.getDecoder();
  } catch (CiffParserException) {
  }

  // Detect camera on filesize (CHDK).
  if (meta != NULL && meta->hasChdkCamera(mInput->getSize())) {
    Camera* c = meta->getChdkCamera(mInput->getSize());

    try {
      return new NakedDecoder(mInput, c);
    } catch (RawDecoderException) {
    }
  }

  // File could not be decoded, so no further options for now.
  ThrowRDE("No decoder found. Sorry.");
  return NULL;
}

} // namespace RawSpeed
