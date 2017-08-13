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
#include <iterator>                                 // for advance
#include <memory>                                   // for unique_ptr
#include <string>                                   // for string, allocator
#include <vector>                                   // for vector

using std::string;

namespace rawspeed {

class CameraMetaData;

bool IiqDecoder::isAppropriateDecoder(const TiffRootIFD* rootIFD,
                                      const Buffer* file) {
  const auto id = rootIFD->getID();
  const std::string& make = id.make;

  const DataBuffer db(*file, Endianness::little);

  return make == "Phase One A/S" && db.get<uint32>(8) == 0x49494949;
}

std::vector<uint32> IiqDecoder::computeSizes(const Buffer& raw_data,
                                             const std::vector<uint32>& offsets,
                                             uint32 height) const {
  assert(height > 0);
  assert(offsets.size() == (1 + height));

  // only for bounds checking
  ByteStream bs(DataBuffer(raw_data, Endianness::little));

  // FIXME: surely there is a nice way to avoid the first copy?
  std::vector<uint32> sortedOffsets(offsets);
  std::sort(sortedOffsets.begin(), sortedOffsets.end());

  std::vector<uint32> sizes;
  sizes.reserve(offsets.size());

  for (auto offset_iterator = std::begin(offsets);
       offset_iterator < std::prev(std::end(offsets));
       std::advance(offset_iterator, 1)) {
    // so... here's the thing. offsets are not guaranteed to be in
    // monotonically increasing order. so for each element of 'offsets',
    // we need to find element which specifies next larger offset.
    // and only then by subtracting those two offsets we get the slice size.

    // find current offset in the sorted vector of offsets
    const auto sorted_iterator = std::find(
        std::begin(sortedOffsets), std::end(sortedOffsets), *offset_iterator);
    assert(sorted_iterator != std::end(sortedOffsets));

    const auto next_sorted_iterator = std::next(sorted_iterator);
    if (next_sorted_iterator == std::end(offsets))
      ThrowRDE("Invalid slice offsets. Corrupt raw.");

    const auto size = *next_sorted_iterator - *sorted_iterator;
    assert(size >= 0);

    if (size == 0)
      ThrowRDE("Invalid slice offsets. Corrupt raw.");

    bs.skipBytes(size); // check that we are still within the raw chunk.

    sizes.emplace_back(size);
  }

  assert(sizes.size() == height);

  return sizes;
}

RawImage IiqDecoder::decodeRawInternal() {
  const Buffer buf(mFile->getSubView(8));
  const DataBuffer db(buf, Endianness::little);
  ByteStream bs(db);

  bs.skipBytes(4); // Phase One magic
  bs.skipBytes(4); // padding?

  const auto origPos = bs.getPosition();

  const uint32 entries_offset = bs.getU32();

  bs.setPosition(entries_offset);

  const uint32 entries_count = bs.getU32();
  bs.skipBytes(4); // ???

  // this is how much is to be read for all the entries
  ByteStream es(bs.getStream(16 * entries_count));

  bs.setPosition(origPos);

  uint32 width = 0;
  uint32 height = 0;

  Buffer raw_data;
  ByteStream block_offsets;
  ByteStream wb;

  for (uint32 entry = 0; entry < entries_count; entry++) {
    const uint32 tag = es.getU32();
    es.skipBytes(4); // type
    const uint32 len = es.getU32();
    const uint32 data = es.getU32();

    switch (tag) {
    case 0x107:
      wb = bs.getSubStream(data, len);
      break;
    case 0x108:
      width = data;
      break;
    case 0x109:
      height = data;
      break;
    case 0x10f:
      raw_data = bs.getSubView(data, len);
      break;
    case 0x21c:
      // they are not guaranteed to be sequential!
      block_offsets = bs.getSubStream(data, len);
      break;
    case 0x21d:
      black_level = data >> 2;
      break;
    default:
      // FIXME: is there a "block_sizes" entry?
      break;
    }
  }

  if (width <= 0 || height <= 0)
    ThrowRDE("couldn't find width and height");

  block_offsets = block_offsets.getStream(height, sizeof(uint32));

  std::vector<uint32> offsets;
  offsets.reserve(1 + height);

  for (uint32 row = 0; row < height; row++)
    offsets.emplace_back(block_offsets.getU32());

  // to simplify slice size calculation, we insert a dummy offset,
  // which will be used much like end()
  offsets.emplace_back(raw_data.getSize());

  std::vector<uint32> sizes(computeSizes(raw_data, offsets, height));

  std::vector<IiqStrip> strips;
  strips.reserve(height);

  for (uint32 row = 0; row < height; row++) {
    // FIXME: is there a "block_sizes" entry?
    const DataBuffer slice(raw_data.getSubView(offsets[row], sizes[row]));
    strips.emplace_back(row, ByteStream(slice));
  }

  mRaw->dim = iPoint2D(width, height);
  mRaw->createData();

  DecodePhaseOneC(strips, width, height);

  for (int i = 0; i < 3; i++)
    mRaw->metadata.wbCoeffs[i] = wb.getFloat();

  return mRaw;
}

void IiqDecoder::DecodePhaseOneC(const std::vector<IiqStrip>& strips,
                                 uint32 width, uint32 height) {
  const int length[] = {8, 7, 6, 9, 11, 10, 5, 12, 14, 13};

  for (const auto& strip : strips) {
    BitPumpMSB32 pump(strip.bs);

    int32 pred[2];
    uint32 len[2];
    pred[0] = pred[1] = 0;
    auto* img = reinterpret_cast<ushort16*>(mRaw->getData(0, strip.n));
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
