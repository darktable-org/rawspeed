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
#include "common/Common.h"                // for uint32, int32, ushort16
#include "common/Point.h"                 // for iPoint2D
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "io/BitPumpMSB32.h"              // for BitPumpMSB32
#include "io/Buffer.h"                    // for Buffer, DataBuffer
#include "io/ByteStream.h"                // for ByteStream
#include "io/Endianness.h"                // for Endianness, Endianness::li...
#include "tiff/TiffIFD.h"                 // for TiffRootIFD, TiffID
#include <algorithm>                      // for move, sort
#include <cassert>                        // for assert
#include <iterator>                       // for advance, begin, end, next
#include <memory>                         // for unique_ptr
#include <string>                         // for operator==, string
#include <vector>                         // for vector

namespace rawspeed {

class CameraMetaData;

bool IiqDecoder::isAppropriateDecoder(const Buffer* file) {
  assert(file);

  const DataBuffer db(*file, Endianness::little);

  // The IIQ magic. Is present for all IIQ raws.
  return db.get<uint32>(8) == 0x49494949;
}

bool IiqDecoder::isAppropriateDecoder(const TiffRootIFD* rootIFD,
                                      const Buffer* file) {
  const auto id = rootIFD->getID();
  const std::string& make = id.make;

  return IiqDecoder::isAppropriateDecoder(file) &&
         (make == "Phase One A/S" || make == "Leaf");
}

// FIXME: this is very close to SamsungV0Decompressor::computeStripes()
std::vector<IiqDecoder::IiqStrip>
IiqDecoder::computeSripes(const Buffer& raw_data,
                          std::vector<IiqOffset>&& offsets,
                          uint32 height) const {
  assert(height > 0);
  assert(offsets.size() == (1 + height));

  ByteStream bs(DataBuffer(raw_data, Endianness::little));

  // so... here's the thing. offsets are not guaranteed to be in
  // monotonically increasing order. so for each element of 'offsets',
  // we need to find element which specifies next larger offset.
  // and only then by subtracting those two offsets we get the slice size.

  std::sort(offsets.begin(), offsets.end(),
            [](const IiqOffset& a, const IiqOffset& b) {
              if (a.offset == b.offset && &a != &b)
                ThrowRDE("Two identical offsets found. Corrupt raw.");
              return a.offset < b.offset;
            });

  std::vector<IiqDecoder::IiqStrip> slices;
  slices.reserve(height);

  auto offset_iterator = std::begin(offsets);
  bs.skipBytes(offset_iterator->offset);

  auto next_offset_iterator = std::next(offset_iterator);
  while (next_offset_iterator < std::end(offsets)) {
    assert(next_offset_iterator->offset > offset_iterator->offset);
    const auto size = next_offset_iterator->offset - offset_iterator->offset;
    assert(size > 0);

    slices.emplace_back(offset_iterator->n, bs.getStream(size));

    std::advance(offset_iterator, 1);
    std::advance(next_offset_iterator, 1);
  }

  assert(slices.size() == height);

  return slices;
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
  uint32 split_row = 0;
  uint32 split_col = 0;

  Buffer raw_data;
  ByteStream block_offsets;
  ByteStream wb;
  ByteStream correction_meta_data;

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
    case 0x110:
      correction_meta_data = bs.getSubStream(data);
      break;
    case 0x21c:
      // they are not guaranteed to be sequential!
      block_offsets = bs.getSubStream(data, len);
      break;
    case 0x21d:
      black_level = data >> 2;
      break;
    case 0x222:
      split_col = data;
      break;
    case 0x224:
      split_row = data;
      break;
    default:
      // FIXME: is there a "block_sizes" entry?
      break;
    }
  }

  // FIXME: could be wrong. max "active pixels" in "Sensor+" mode - "101 MP"
  if (width == 0 || height == 0 || width > 11608 || height > 8708)
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", width, height);

  block_offsets = block_offsets.getStream(height, sizeof(uint32));

  std::vector<IiqOffset> offsets;
  offsets.reserve(1 + height);

  for (uint32 row = 0; row < height; row++)
    offsets.emplace_back(row, block_offsets.getU32());

  // to simplify slice size calculation, we insert a dummy offset,
  // which will be used much like end()
  offsets.emplace_back(height, raw_data.getSize());

  const std::vector<IiqStrip> strips(
      computeSripes(raw_data, std::move(offsets), height));

  mRaw->dim = iPoint2D(width, height);
  mRaw->createData();

  DecodePhaseOneC(strips, width, height);
  if (correction_meta_data.getSize() != 0) {
    CorrectPhaseOneC(correction_meta_data, split_row, split_col);
  }

  for (int i = 0; i < 3; i++)
    mRaw->metadata.wbCoeffs[i] = wb.getFloat();

  return mRaw;
}

void IiqDecoder::DecodeStrip(const IiqStrip& strip, uint32 width,
                             uint32 height) {
  const int length[] = {8, 7, 6, 9, 11, 10, 5, 12, 14, 13};

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
        int j = 0;

        for (; j < 5; j++) {
          if (pump.getBits(1) != 0) {
            if (col == 0)
              ThrowRDE("Can not initialize lenghts. Data is corrupt.");

            // else, we have previously initialized lenghts, so we are fine
            break;
          }
        }

        assert((col == 0 && j > 0) || col != 0);
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

void IiqDecoder::DecodePhaseOneC(const std::vector<IiqStrip>& strips,
                                 uint32 width, uint32 height) {
  for (const auto& strip : strips)
    DecodeStrip(strip, width, height);
}

void IiqDecoder::CorrectPhaseOneC(ByteStream meta_data, uint32 split_row,
                                  uint32 split_col) {
  ByteStream meta_data_base(meta_data.getSubStream(0));

  meta_data.skipBytes(8);
  const uint32 bytes_to_entries = meta_data.getU32();
  meta_data.skipBytes(bytes_to_entries - 12);
  const uint32 entries_count = meta_data.getU32();
  meta_data.skipBytes(4);

  // this is how much is to be read for all the entries
  ByteStream entries(meta_data.getStream(16 * entries_count));

  bool quadrant_multiplier_applied = false;

  for (uint32 entry = 0; entry < entries_count; entry++) {
    const uint32 tag = entries.getU32();
    const uint32 len = entries.getU32();
    const uint32 data = entries.getU32();

    switch (tag) {
    case 0x431:
      if (!quadrant_multiplier_applied) {
        CorrectQuadrantMultipliersCombined(
            meta_data_base.getSubStream(data, len), split_row, split_col);
      }
      quadrant_multiplier_applied = true;
      break;
    default:
      break;
    }
  }
}

void IiqDecoder::CorrectQuadrantMultipliersCombined(ByteStream data,
                                                    uint32 split_row,
                                                    uint32 split_col) {
  // TODO: Implementation

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
