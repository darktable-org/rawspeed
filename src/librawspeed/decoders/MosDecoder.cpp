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

#include "decoders/MosDecoder.h"
#include "common/Common.h"                          // for uint32, uchar8
#include "common/Point.h"                           // for iPoint2D
#include "decoders/RawDecoder.h"                    // for RawDecoder
#include "decoders/RawDecoderException.h"           // for RawDecoderExcept...
#include "decompressors/UncompressedDecompressor.h" // for UncompressedDeco...
#include "io/BitPumpMSB32.h"                        // for BitPumpMSB32
#include "io/Buffer.h"                              // for Buffer
#include "io/ByteStream.h"                          // for ByteStream
#include "io/Endianness.h"                          // for getU32LE, getLE
#include "parsers/TiffParserException.h"            // for TiffParserException
#include "tiff/TiffEntry.h"                         // for TiffEntry
#include "tiff/TiffIFD.h"                           // for TiffRootIFD, Tif...
#include "tiff/TiffTag.h"                           // for TiffTag::TILEOFF...
#include <algorithm>                                // for move
#include <cassert>                                  // for assert
#include <cstring>                                  // for memchr
#include <istream>                                  // for istringstream
#include <memory>                                   // for unique_ptr
#include <string>                                   // for string, allocator

using std::string;

namespace rawspeed {

class CameraMetaData;

bool MosDecoder::isAppropriateDecoder(const TiffRootIFD* rootIFD,
                                      const Buffer* file) {
  try {
    const auto id = rootIFD->getID();
    const std::string& make = id.make;

    // FIXME: magic

    return make == "Leaf" || make == "Phase One A/S";
  } catch (const TiffParserException&) {
    // Last ditch effort to identify Leaf cameras that don't have a Tiff Make
    // set
    TiffEntry* softwareIFD = rootIFD->getEntryRecursive(SOFTWARE);
    if (!softwareIFD)
      return false;

    const string software = trimSpaces(softwareIFD->getString());
    return software == "Camera Library";
  }
}

MosDecoder::MosDecoder(TiffRootIFDOwner&& rootIFD, const Buffer* file)
    : AbstractTiffDecoder(move(rootIFD), file) {
  black_level = 0;

  if (mRootIFD->getEntryRecursive(MAKE)) {
    auto id = mRootIFD->getID();
    make = id.make;
    model = id.model;
  } else {
    TiffEntry *xmp = mRootIFD->getEntryRecursive(XMP);
    if (!xmp)
      ThrowRDE("Couldn't find the XMP");

    assert(xmp != nullptr);
    string xmpText = xmp->getString();
    make = getXMPTag(xmpText, "Make");
    model = getXMPTag(xmpText, "Model");
  }
}

string MosDecoder::getXMPTag(const string &xmp, const string &tag) {
  string::size_type start = xmp.find("<tiff:"+tag+">");
  string::size_type end = xmp.find("</tiff:"+tag+">");
  if (start == string::npos || end == string::npos || end <= start)
    ThrowRDE("Couldn't find tag '%s' in the XMP", tag.c_str());
  int startlen = tag.size()+7;
  return xmp.substr(start+startlen, end-start-startlen);
}

RawImage MosDecoder::decodeRawInternal() {
  uint32 off = 0;

  uint32 base = 8;
  // We get a pointer up to the end of the file as we check offset bounds later
  const uchar8 *insideTiff = mFile->getData(base, mFile->getSize()-base);
  if (getU32LE(insideTiff) == 0x49494949) {
    uint32 offset = getU32LE(insideTiff + 8);
    if (offset+base+4 > mFile->getSize())
      ThrowRDE("offset out of bounds");

    uint32 entries = getU32LE(insideTiff + offset);
    uint32 pos = 8; // Skip another 4 bytes

    uint32 width = 0;
    uint32 height = 0;
    uint32 strip_offset = 0;
    uint32 data_offset = 0;
    uint32 wb_offset = 0;
    for (; entries > 0; entries--) {
      if (offset+base+pos+16 > mFile->getSize())
        ThrowRDE("offset out of bounds");

      uint32 tag = getU32LE(insideTiff + offset + pos + 0);
      // uint32 type = getU32LE(insideTiff + offset + pos + 4);
      // uint32 len  = getU32LE(insideTiff + offset + pos + 8);
      uint32 data = getU32LE(insideTiff + offset + pos + 12);
      pos += 16;
      switch (tag) {
      case 0x107:
        wb_offset = data + base;
        break;
      case 0x108:
        width = data;
        break;
      case 0x109:
        height = data;
        break;
      case 0x10f:
        data_offset = data + base;
        break;
      case 0x21c:
        strip_offset = data + base;
        break;
      case 0x21d:
        black_level = data >> 2;
        break;
      default:
        break;
      }
    }
    if (width <= 0 || height <= 0)
      ThrowRDE("couldn't find width and height");
    if (strip_offset+height*4 > mFile->getSize())
      ThrowRDE("strip offsets out of bounds");
    if (data_offset > mFile->getSize())
      ThrowRDE("data offset out of bounds");

    mRaw->dim = iPoint2D(width, height);
    mRaw->createData();

    DecodePhaseOneC(data_offset, strip_offset, width, height);

    const uchar8 *data = mFile->getData(wb_offset, 12);
    for(int i=0; i<3; i++) {
      mRaw->metadata.wbCoeffs[i] = getLE<float>(data + i * 4);
    }

    return mRaw;
  }

  const TiffIFD *raw = nullptr;

  if (mRootIFD->hasEntryRecursive(TILEOFFSETS)) {
    raw = mRootIFD->getIFDWithTag(TILEOFFSETS);
    off = raw->getEntry(TILEOFFSETS)->getU32();
  } else {
    raw = mRootIFD->getIFDWithTag(CFAPATTERN);
    off = raw->getEntry(STRIPOFFSETS)->getU32();
  }

  uint32 width = raw->getEntry(IMAGEWIDTH)->getU32();
  uint32 height = raw->getEntry(IMAGELENGTH)->getU32();
  mRaw->dim = iPoint2D(width, height);
  mRaw->createData();

  UncompressedDecompressor u(*mFile, off, mRaw);

  int compression = raw->getEntry(COMPRESSION)->getU32();
  if (1 == compression) {
    if (getTiffEndianness(mFile) == big)
      u.decodeRawUnpacked<16, big>(width, height);
    else
      u.decodeRawUnpacked<16, little>(width, height);
  }
  else if (99 == compression || 7 == compression) {
    ThrowRDE("Leaf LJpeg not yet supported");
    //LJpegPlain l(mFile, mRaw);
    //l.startDecoder(off, mFile->getSize()-off, 0, 0);
  } else
    ThrowRDE("Unsupported compression: %d", compression);

  return mRaw;
}

void MosDecoder::DecodePhaseOneC(uint32 data_offset, uint32 strip_offset, uint32 width, uint32 height)
{
  const int length[] = { 8,7,6,9,11,10,5,12,14,13 };

  for (uint32 row = 0; row < height; row++) {
    uint32 off =
        data_offset + getU32LE(mFile->getData(strip_offset + row * 4, 4));

    BitPumpMSB32 pump(mFile, off);
    int32 pred[2];
    uint32 len[2];
    pred[0] = pred[1] = 0;
    auto* img = reinterpret_cast<ushort16*>(mRaw->getData(0, row));
    for (uint32 col=0; col < width; col++) {
      if (col >= (width & -8))
        len[0] = len[1] = 14;
      else if ((col & 7) == 0) {
        for (unsigned int &i : len) {
          int32 j = 0;

          for (; j < 5; j++)
            if (pump.getBits(1) != 0)
              break;

          if (j > 0)
            i = length[2 * (j - 1) + pump.getBits(1)];
        }
      }

      int i = len[col & 1];
      if (i == 14)
        img[col] = pred[col & 1] = pump.getBits(16);
      else
        img[col] = pred[col & 1] +=
            static_cast<signed>(pump.getBits(i)) + 1 - (1 << (i - 1));
    }
  }
}

void MosDecoder::checkSupportInternal(const CameraMetaData* meta) {
  RawDecoder::checkCameraSupported(meta, make, model, "");
}

void MosDecoder::decodeMetaDataInternal(const CameraMetaData* meta) {
  RawDecoder::setMetaData(meta, make, model, "", 0);

  // Fetch the white balance (see dcraw.c parse_mos for more metadata that can be gotten)
  if (mRootIFD->hasEntryRecursive(LEAFMETADATA)) {
    ByteStream bs = mRootIFD->getEntryRecursive(LEAFMETADATA)->getData();

    // We need at least a couple of bytes:
    // "NeutObj_neutrals" + 28 bytes binay + 4x uint as strings + 3x space + \0
    const uint32 minSize = 16+28+4+3+1;

    // dcraw does actual parsing, since we just want one field we bruteforce it
    while (bs.getRemainSize() > minSize) {
      if (bs.skipPrefix("NeutObj_neutrals", 16)) {
        bs.skipBytes(28);
        // check for nulltermination of string inside bounds
        if (!memchr(bs.peekData(bs.getRemainSize()), 0, bs.getRemainSize()))
          break;
        uint32 tmp[4] = {0};
        std::istringstream iss(bs.peekString());
        iss >> tmp[0] >> tmp[1] >> tmp[2] >> tmp[3];
        if (!iss.fail() && tmp[0] > 0 && tmp[1] > 0 && tmp[2] > 0 &&
            tmp[3] > 0) {
          mRaw->metadata.wbCoeffs[0] = static_cast<float>(tmp[0]) / tmp[1];
          mRaw->metadata.wbCoeffs[1] = static_cast<float>(tmp[0]) / tmp[2];
          mRaw->metadata.wbCoeffs[2] = static_cast<float>(tmp[0]) / tmp[3];
        }
        break;
      }
      bs.skipBytes(1);
    }
  }

  if (black_level)
    mRaw->blackLevel = black_level;
}

} // namespace rawspeed
