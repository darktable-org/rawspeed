/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2014-2015 Pedro Côrte-Real
    Copyright (C) 2017-2019 Roman Lebedev
    Copyright (C) 2019 Robert Bridge

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
#include "common/Array2DRef.h"                  // for Array2DRef
#include "common/Mutex.h"                       // for MutexLocker
#include "common/Point.h"                       // for iPoint2D
#include "common/Spline.h"                      // for Spline, Spline<>::va...
#include "decoders/RawDecoder.h"                // for RawDecoder::(anonymous)
#include "decoders/RawDecoderException.h"       // for ThrowRDE
#include "decompressors/PhaseOneDecompressor.h" // for PhaseOneStrip, Phase...
#include "io/Buffer.h"                          // for Buffer, DataBuffer
#include "io/ByteStream.h"                      // for ByteStream
#include "io/Endianness.h"                      // for Endianness, Endianne...
#include "metadata/Camera.h"                    // for Camera
#include "metadata/CameraMetaData.h"            // for CameraMetaData
#include "metadata/ColorFilterArray.h"          // for ColorFilterArray
#include "tiff/TiffIFD.h"                       // for TiffID, TiffRootIFD
#include <algorithm>                            // for adjacent_find, gener...
#include <array>                                // for array, array<>::cons...
#include <cassert>                              // for assert
#include <cinttypes>                            // for PRIu64
#include <cmath>                                // for lround
#include <cstdlib>                              // for abs
#include <functional>                           // for greater_equal
#include <iterator>                             // for advance, next, begin
#include <memory>                               // for unique_ptr
#include <string>                               // for operator==, string
#include <utility>                              // for move
#include <vector>                               // for vector

namespace rawspeed {

bool IiqDecoder::isAppropriateDecoder(const Buffer& file) {
  const DataBuffer db(file, Endianness::little);

  // The IIQ magic. Is present for all IIQ raws.
  return db.get<uint32_t>(8) == 0x49494949;
}

bool IiqDecoder::isAppropriateDecoder(const TiffRootIFD* rootIFD,
                                      const Buffer& file) {
  const auto id = rootIFD->getID();
  const std::string& make = id.make;

  return IiqDecoder::isAppropriateDecoder(file) &&
         (make == "Phase One A/S" || make == "Phase One" || make == "Leaf");
}

// FIXME: this is very close to SamsungV0Decompressor::computeStripes()
std::vector<PhaseOneStrip>
IiqDecoder::computeSripes(const Buffer& raw_data,
                          std::vector<IiqOffset> offsets, uint32_t height) {
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

  std::vector<PhaseOneStrip> slices;
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

void IiqDecoder::decodeRawInternal() {
  const Buffer buf(mFile.getSubView(8));
  const DataBuffer db(buf, Endianness::little);
  ByteStream bs(db);

  bs.skipBytes(4); // Phase One magic
  bs.skipBytes(4); // padding?

  const auto origPos = bs.getPosition();

  const uint32_t entries_offset = bs.getU32();

  bs.setPosition(entries_offset);

  const uint32_t entries_count = bs.getU32();
  bs.skipBytes(4); // ???

  // this is how much is to be read for all the entries
  ByteStream es(bs.getStream(entries_count, 16));

  bs.setPosition(origPos);

  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t split_row = 0;
  uint32_t split_col = 0;

  Buffer raw_data;
  ByteStream block_offsets;
  ByteStream wb;
  ByteStream correction_meta_data;

  for (uint32_t entry = 0; entry < entries_count; entry++) {
    const uint32_t tag = es.getU32();
    es.skipBytes(4); // type
    const uint32_t len = es.getU32();
    const uint32_t data = es.getU32();

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
  if (width == 0 || height == 0 || width > 11976 || height > 8854)
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", width, height);

  if (split_col > width || split_row > height)
    ThrowRDE("Invalid sensor quadrant split values (%u, %u)", split_row,
             split_col);

  block_offsets = block_offsets.getStream(height, sizeof(uint32_t));

  std::vector<IiqOffset> offsets;
  offsets.reserve(1 + height);

  for (uint32_t row = 0; row < height; row++)
    offsets.emplace_back(row, block_offsets.getU32());

  // to simplify slice size calculation, we insert a dummy offset,
  // which will be used much like end()
  offsets.emplace_back(height, raw_data.getSize());

  std::vector<PhaseOneStrip> strips(
      computeSripes(raw_data, std::move(offsets), height));

  mRaw.get(0)->dim = iPoint2D(width, height);

  PhaseOneDecompressor p(mRaw.get(0).get(), std::move(strips));
  mRaw.get(0)->createData();
  p.decompress();

  if (correction_meta_data.getSize() != 0 && iiq)
    CorrectPhaseOneC(correction_meta_data, split_row, split_col);

  for (int i = 0; i < 3; i++)
    mRaw.metadata.wbCoeffs[i] = wb.getFloat();
}

void IiqDecoder::CorrectPhaseOneC(ByteStream meta_data, uint32_t split_row,
                                  uint32_t split_col) {
  meta_data.skipBytes(8);
  const uint32_t bytes_to_entries = meta_data.getU32();
  meta_data.setPosition(bytes_to_entries);
  const uint32_t entries_count = meta_data.getU32();
  meta_data.skipBytes(4);

  // this is how much is to be read for all the entries
  ByteStream entries(meta_data.getStream(entries_count, 12));
  meta_data.setPosition(0);

  bool QuadrantMultipliersSeen = false;
  bool SensorDefectsSeen = false;

  for (uint32_t entry = 0; entry < entries_count; entry++) {
    const uint32_t tag = entries.getU32();
    const uint32_t len = entries.getU32();
    const uint32_t offset = entries.getU32();

    switch (tag) {
    case 0x400: // Sensor Defects
      if (SensorDefectsSeen)
        ThrowRDE("Second sensor defects entry seen. Unexpected.");
      correctSensorDefects(meta_data.getSubStream(offset, len));
      SensorDefectsSeen = true;
      break;
    case 0x431:
      if (QuadrantMultipliersSeen)
        ThrowRDE("Second quadrant multipliers entry seen. Unexpected.");
      if (iiq.quadrantMultipliers)
        CorrectQuadrantMultipliersCombined(meta_data.getSubStream(offset, len),
                                           split_row, split_col);
      QuadrantMultipliersSeen = true;
      break;
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
                                                    uint32_t split_row,
                                                    uint32_t split_col) const {
  std::array<uint32_t, 9> shared_x_coords;

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
        const uint64_t y_coord =
            (uint64_t(data.getU32()) * shared_x_coords[i]) / 10000ULL;
        if (y_coord > 65535)
          ThrowRDE("The Y coordinate %" PRIu64 " is too large", y_coord);
        quadrant.emplace_back(shared_x_coords[i], y_coord);
      }

      quadrant.emplace_back(65535, 65535);
      assert(quadrant.size() == 9);
    }
  }

  for (int quadRow = 0; quadRow < 2; quadRow++) {
    for (int quadCol = 0; quadCol < 2; quadCol++) {
      auto *rawU16 = dynamic_cast<RawImageDataU16*>(mRaw.get(0).get());
      assert(rawU16);
      const Array2DRef<uint16_t> img(rawU16->getU16DataAsUncroppedArray2DRef());

      const Spline<> s(control_points[quadRow][quadCol]);
      const std::vector<uint16_t> curve = s.calculateCurve();

      int row_start = quadRow == 0 ? 0 : split_row;
      int row_end = quadRow == 0 ? split_row : img.height;
      int col_start = quadCol == 0 ? 0 : split_col;
      int col_end = quadCol == 0 ? split_col : img.width;

      for (int row = row_start; row < row_end; row++) {
        for (int col = col_start; col < col_end; col++) {
          uint16_t& pixel = img(row, col);
          // This adjustment is expected to be made with the
          // black-level already subtracted from the pixel values.
          // Because this is kept as metadata and not subtracted at
          // this point, to make the correction work we subtract the
          // appropriate amount before indexing into the curve and
          // then add it back so that subtracting the black level
          // later will work as expected
          const uint16_t diff = pixel < black_level ? pixel : black_level;
          pixel = curve[pixel - diff] + diff;
        }
      }
    }
  }
}

void IiqDecoder::checkSupportInternal(const CameraMetaData* meta) {
  checkCameraSupported(meta, mRootIFD->getID(), "");

  auto id = mRootIFD->getID();
  const Camera* cam = meta->getCamera(id.make, id.model, mRaw.metadata.mode);
  if (!cam)
    ThrowRDE("Couldn't find camera %s %s", id.make.c_str(), id.model.c_str());

  mRaw.get(0)->cfa = cam->cfa;
}

void IiqDecoder::decodeMetaDataInternal(const CameraMetaData* meta) {
  setMetaData(meta, "", 0);

  if (black_level)
    mRaw.get(0)->blackLevel = black_level;
}

void IiqDecoder::correctSensorDefects(ByteStream data) const {
  while (data.getRemainSize() != 0) {
    const uint16_t col = data.getU16();
    const uint16_t row = data.getU16();
    const uint16_t type = data.getU16();
    data.skipBytes(2); // Ignore unknown/unused bits.

    if (col >= mRaw.get(0)->dim.x) // Value for col is outside the raw image.
      continue;
    switch (type) {
    case 131: // bad column
    case 137: // bad column
      correctBadColumn(col);
      break;
    case 129: // bad pixel
      handleBadPixel(col, row);
      break;
    default: // Oooh, a sensor defect not in dcraw!
      break;
    }
  }
}

void IiqDecoder::handleBadPixel(const uint16_t col, const uint16_t row) const {
  MutexLocker guard(&mRaw.get(0)->mBadPixelMutex);
  mRaw.get(0)->mBadPixelPositions.insert(mRaw.get(0)->mBadPixelPositions.end(),
                                  (static_cast<uint32_t>(row) << 16) + col);
}

void IiqDecoder::correctBadColumn(const uint16_t col) const {
  auto *rawU16 = dynamic_cast<RawImageDataU16*>(mRaw.get(0).get());
  assert(rawU16);
  const Array2DRef<uint16_t> img(rawU16->getU16DataAsUncroppedArray2DRef());

  for (int row = 2; row < mRaw.get(0)->dim.y - 2; row++) {
    if (mRaw.get(0)->cfa.getColorAt(col, row) == CFAColor::GREEN) {
      /* Do green pixels. Let's pretend we are in "G" pixel, in the middle:
       *   G=G
       *   BGB
       *   G0G
       * We accumulate the values 4 "G" pixels form diagonals, then check which
       * of 4 values is most distant from the mean of those 4 values, subtract
       * it from the sum, average (divide by 3) and round to nearest int.
       */
      int max = 0;
      std::array<uint16_t, 4> val;
      std::array<int32_t, 4> dev;
      int32_t sum = 0;
      sum += val[0] = img(row - 1, col - 1);
      sum += val[1] = img(row + 1, col - 1);
      sum += val[2] = img(row - 1, col + 1);
      sum += val[3] = img(row + 1, col + 1);
      for (int i = 0; i < 4; i++) {
        dev[i] = std::abs((val[i] * 4) - sum);
        if (dev[max] < dev[i])
          max = i;
      }
      const int three_pixels = sum - val[max];
      // This is `std::lround(three_pixels / 3.0)`, but without FP.
      img(row, col) = (three_pixels + 1) / 3;
    } else {
      /*
       * Do non-green pixels. Let's pretend we are in "R" pixel, in the middle:
       *   RG=GR
       *   GB=BG
       *   RGRGR
       *   GB0BG
       *   RG0GR
       * We have 6 other "R" pixels - 2 by horizontal, 4 by diagonals.
       * We need to combine them, to get the value of the pixel we are in.
       */
      uint32_t diags = img(row + 2, col - 2) + img(row - 2, col - 2) +
                       img(row + 2, col + 2) + img(row - 2, col + 2);
      uint32_t horiz = img(row, col - 2) + img(row, col + 2);
      // But this is not just averaging, we bias towards the horizontal pixels.
      img(row, col) = std::lround(diags * 0.0732233 + horiz * 0.3535534);
    }
  }
}

} // namespace rawspeed
