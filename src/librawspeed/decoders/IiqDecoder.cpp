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

#include "decoders/IiqDecoder.h"
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

bool IiqDecoder::isAppropriateDecoder(const TiffRootIFD* rootIFD,
                                      const Buffer* file) {
  const auto id = rootIFD->getID();
  const std::string& make = id.make;

  // FIXME: magic

  return make == "Phase One A/S";
}

RawImage IiqDecoder::decodeRawInternal() {
  uint32 base = 8;
  // We get a pointer up to the end of the file as we check offset bounds later
  const uchar8* insideTiff = mFile->getData(base, mFile->getSize() - base);
  if (getU32LE(insideTiff) != 0x49494949)
    ThrowRDE("Not IIQ. Why are you calling me?");

  uint32 offset = getU32LE(insideTiff + 8);
  if (offset + base + 4 > mFile->getSize())
    ThrowRDE("offset out of bounds");

  uint32 entries = getU32LE(insideTiff + offset);
  uint32 pos = 8; // Skip another 4 bytes

  uint32 width = 0;
  uint32 height = 0;
  uint32 strip_offset = 0;
  uint32 data_offset = 0;
  uint32 wb_offset = 0;
  for (; entries > 0; entries--) {
    if (offset + base + pos + 16 > mFile->getSize())
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
  if (strip_offset + height * 4 > mFile->getSize())
    ThrowRDE("strip offsets out of bounds");
  if (data_offset > mFile->getSize())
    ThrowRDE("data offset out of bounds");

  mRaw->dim = iPoint2D(width, height);
  mRaw->createData();

  DecodePhaseOneC(data_offset, strip_offset, width, height);

  const uchar8* data = mFile->getData(wb_offset, 12);
  for (int i = 0; i < 3; i++) {
    mRaw->metadata.wbCoeffs[i] = getLE<float>(data + i * 4);
  }

  return mRaw;
}

void IiqDecoder::DecodePhaseOneC(uint32 data_offset, uint32 strip_offset,
                                 uint32 width, uint32 height) {
  const int length[] = {8, 7, 6, 9, 11, 10, 5, 12, 14, 13};

  for (uint32 row = 0; row < height; row++) {
    uint32 off =
        data_offset + getU32LE(mFile->getData(strip_offset + row * 4, 4));

    BitPumpMSB32 pump(mFile, off);
    int32 pred[2];
    uint32 len[2];
    pred[0] = pred[1] = 0;
    auto* img = reinterpret_cast<ushort16*>(mRaw->getData(0, row));
    for (uint32 col = 0; col < width; col++) {
      if (col >= (width & -8))
        len[0] = len[1] = 14;
      else if ((col & 7) == 0) {
        for (unsigned int& i : len) {
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

void IiqDecoder::checkSupportInternal(const CameraMetaData* meta) {
  checkCameraSupported(meta, mRootIFD->getID(), "");
}

void IiqDecoder::decodeMetaDataInternal(const CameraMetaData* meta) {
  setMetaData(meta, "", 0);

  if (black_level)
    mRaw->blackLevel = black_level;
}

} // namespace rawspeed
