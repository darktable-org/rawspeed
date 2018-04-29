/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2014-2015 Pedro Côrte-Real
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

#include "decoders/IiqDecoder.h"
#include "common/Common.h"                // for uint32, ushort16, int32
#include "common/Point.h"                 // for iPoint2D
#include "common/Spline.h"                // for Spline, Spline<>::value_type
#include "decoders/RawDecoder.h"          // for RawDecoder::(anonymous)
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "io/BitPumpMSB32.h"              // for BitPumpMSB32
#include "io/Buffer.h"                    // for Buffer, DataBuffer
#include "io/ByteStream.h"                // for ByteStream
#include "io/Endianness.h"                // for Endianness, Endianness::li...
#include "tiff/TiffIFD.h"                 // for TiffRootIFD, TiffID
#include <algorithm>                      // for adjacent_find, generate_n
#include <array>                          // for array, array<>::const_iter...
#include <cassert>                        // for assert
#include <functional>                     // for greater_equal
#include <iterator>                       // for advance, next, begin, end
#include <memory>                         // for unique_ptr
#include <string>                         // for operator==, string
#include <utility>                        // for move
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
  ByteStream es(bs.getStream(entries_count, 16));

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

  if (split_col > width || split_row > height)
    ThrowRDE("Invalid sensor quadrant split values (%u, %u)", split_row,
             split_col);

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
  if (correction_meta_data.getSize() != 0 && iiq)
    CorrectPhaseOneC(correction_meta_data, split_row, split_col);

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
              ThrowRDE("Can not initialize lengths. Data is corrupt.");

            // else, we have previously initialized lengths, so we are fine
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
  meta_data.skipBytes(8);
  const uint32 bytes_to_entries = meta_data.getU32();
  meta_data.setPosition(bytes_to_entries);
  const uint32 entries_count = meta_data.getU32();
  meta_data.skipBytes(4);

  // this is how much is to be read for all the entries
  ByteStream entries(meta_data.getStream(entries_count, 12));
  meta_data.setPosition(0);

  for (uint32 entry = 0; entry < entries_count; entry++) {
    const uint32 tag = entries.getU32();
    const uint32 len = entries.getU32();
    const uint32 offset = entries.getU32();

    switch (tag) {
    case 0x431:
      if (!iiq.quadrantMultipliers)
        return;

      CorrectQuadrantMultipliersCombined(meta_data.getSubStream(offset, len),
                                         split_row, split_col);
      return;
    default:
      break;
    }
  }
}

// This method defines a correction that compensates for the fact that
// IIQ files may come from a camera with multiple (four, in this case)
// sensors combined into a single "sensor."  Because the different
// sensors may have slightly different responses, we need to multiply
// the pixels in each by a correction factor to ensure that they blend
// together smoothly.  The correction factor is not a single
// multiplier, but a curve defined by seven control points.  Each
// curve's control points share the same seven X-coordinates.
void IiqDecoder::CorrectQuadrantMultipliersCombined(ByteStream data,
                                                    uint32 split_row,
                                                    uint32 split_col) {
  std::array<uint32, 9> shared_x_coords;

  // Read the middle seven points from the file
  std::generate_n(std::next(shared_x_coords.begin()), 7,
                  [&data] { return data.getU32(); });

  // All the curves include (0, 0) and (65535, 65535),
  // so the first and last points are predefined
  shared_x_coords.front() = 0;
  shared_x_coords.back() = 65535;

  // Check that the middle coordinates make sense.
  if (std::adjacent_find(shared_x_coords.cbegin(), shared_x_coords.cend(),
                         std::greater_equal<>()) != shared_x_coords.cend())
    ThrowRDE("The X coordinates must all be strictly increasing");

  std::array<std::array<std::vector<iPoint2D>, 2>, 2> control_points;
  for (auto& quadRow : control_points) {
    for (auto& quadrant : quadRow) {
      quadrant.reserve(9);
      quadrant.emplace_back(0, 0);

      for (int i = 1; i < 8; i++) {
        // These multipliers are expressed in ten-thousandths in the
        // file
        const uint64 y_coord =
            (uint64(data.getU32()) * shared_x_coords[i]) / 10000ULL;
        if (y_coord > 65535)
          ThrowRDE("The Y coordinate %llu is too large", y_coord);
        quadrant.emplace_back(shared_x_coords[i], y_coord);
      }

      quadrant.emplace_back(65535, 65535);
      assert(quadrant.size() == 9);
    }
  }

  for (int quadRow = 0; quadRow < 2; quadRow++) {
    for (int quadCol = 0; quadCol < 2; quadCol++) {
      const Spline<> s(control_points[quadRow][quadCol]);
      const std::vector<ushort16> curve = s.calculateCurve();

      int row_start = quadRow == 0 ? 0 : split_row;
      int row_end = quadRow == 0 ? split_row : mRaw->dim.y;
      int col_start = quadCol == 0 ? 0 : split_col;
      int col_end = quadCol == 0 ? split_col : mRaw->dim.x;

      for (int row = row_start; row < row_end; row++) {
        auto* pixel =
            reinterpret_cast<ushort16*>(mRaw->getData(col_start, row));
        for (int col = col_start; col < col_end; col++, pixel++) {
          // This adjustment is expected to be made with the
          // black-level already subtracted from the pixel values.
          // Because this is kept as metadata and not subtracted at
          // this point, to make the correction work we subtract the
          // appropriate amount before indexing into the curve and
          // then add it back so that subtracting the black level
          // later will work as expected
          const ushort16 diff = *pixel < black_level ? *pixel : black_level;
          *pixel = curve[*pixel - diff] + diff;
        }
      }
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
