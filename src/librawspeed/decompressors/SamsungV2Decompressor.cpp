/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2010 Klaus Post
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

#include "decompressors/SamsungV2Decompressor.h"
#include "common/Common.h"                // for uint32_t, uint16_t, int32_t
#include "common/Point.h"                 // for iPoint2D
#include "common/RawImage.h"              // for RawImage, RawImageData
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "io/BitPumpMSB32.h"              // for BitPumpMSB32
#include "io/ByteStream.h"                // for ByteStream
#include <algorithm>                      // for max
#include <cassert>                        // for assert
#include <type_traits>                    // for underlying_type, underlyin...

namespace rawspeed {

// Seriously Samsung just use lossless jpeg already, it compresses better too :)

// Thanks to Michael Reichmann (Luminous Landscape) for putting Pedro Côrte-Real
// in contact and Loring von Palleske (Samsung) for pointing to the open-source
// code of Samsung's DNG converter at http://opensource.samsung.com/

enum struct SamsungV2Decompressor::OptFlags : uint32_t {
  NONE = 0U,       // no flags
  SKIP = 1U << 0U, // Skip checking if we need differences from previous line
  MV = 1U << 1U,   // Simplify motion vector definition
  QP = 1U << 2U,   // Don't scale the diff values

  // all possible flags
  ALL = SKIP | MV | QP,
};

constexpr SamsungV2Decompressor::OptFlags
operator|(SamsungV2Decompressor::OptFlags lhs,
          SamsungV2Decompressor::OptFlags rhs) {
  return static_cast<SamsungV2Decompressor::OptFlags>(
      static_cast<std::underlying_type<SamsungV2Decompressor::OptFlags>::type>(
          lhs) |
      static_cast<std::underlying_type<SamsungV2Decompressor::OptFlags>::type>(
          rhs));
}

constexpr bool operator&(SamsungV2Decompressor::OptFlags lhs,
                         SamsungV2Decompressor::OptFlags rhs) {
  return SamsungV2Decompressor::OptFlags::NONE !=
         static_cast<SamsungV2Decompressor::OptFlags>(
             static_cast<
                 std::underlying_type<SamsungV2Decompressor::OptFlags>::type>(
                 lhs) &
             static_cast<
                 std::underlying_type<SamsungV2Decompressor::OptFlags>::type>(
                 rhs));
}

inline __attribute__((always_inline)) int16_t
SamsungV2Decompressor::getDiff(BitPumpMSB32* pump, uint32_t len) {
  if (len == 0)
    return 0;
  assert(len <= 15 && "Difference occupies at most 15 bits.");
  return signExtend(pump->getBits(len), len);
}

SamsungV2Decompressor::SamsungV2Decompressor(const RawImage& image,
                                             const ByteStream& bs, int bit)
    : AbstractSamsungDecompressor(image), bits(bit) {
  if (mRaw->getCpp() != 1 || mRaw->getDataType() != TYPE_USHORT16 ||
      mRaw->getBpp() != 2)
    ThrowRDE("Unexpected component count / data type");

  switch (bit) {
  case 12:
  case 14:
    break;
  default:
    ThrowRDE("Unexpected bit per pixel (%u)", bit);
  }

  static constexpr const auto headerSize = 16;
  bs.check(headerSize);

  BitPumpMSB32 startpump(bs);

  // Process the initial metadata bits, we only really use initVal, width and
  // height (the last two match the TIFF values anyway)
  startpump.getBits(16); // NLCVersion
  startpump.getBits(4);  // ImgFormat
  bitDepth = startpump.getBits(4) + 1;
  if (bitDepth != 12 && bitDepth != 14)
    ThrowRDE("Unexpected bit depth %u, expected 12 or 14.", bitDepth);
  startpump.getBits(4); // NumBlkInRCUnit
  startpump.getBits(4); // CompressionRatio
  width = startpump.getBits(16);
  height = startpump.getBits(16);
  startpump.getBits(16); // TileWidth
  startpump.getBits(4);  // reserved

  // The format includes an optimization code that sets 3 flags to change the
  // decoding parameters
  const uint32_t optflags = startpump.getBits(4);
  if (optflags > static_cast<uint32_t>(OptFlags::ALL))
    ThrowRDE("Invalid opt flags %x", optflags);
  _flags = static_cast<OptFlags>(optflags);

  startpump.getBits(8); // OverlapWidth
  startpump.getBits(8); // reserved
  startpump.getBits(8); // Inc
  startpump.getBits(2); // reserved
  initVal = startpump.getBits(14);

  assert(startpump.getPosition() == headerSize);

  if (width == 0 || height == 0 || width % 16 != 0 || width > 6496 ||
      height > 4336)
    ThrowRDE("Unexpected image dimensions found: (%i; %i)", width, height);

  if (width != mRaw->dim.x || height != mRaw->dim.y)
    ThrowRDE("EXIF image dimensions do not match dimensions from raw header");

  data = startpump.getStream(startpump.getRemainSize());
}

void SamsungV2Decompressor::decompress() {
  switch (_flags) {
  case OptFlags::NONE:
    for (int row = 0; row < height; row++)
      decompressRow<OptFlags::NONE>(row);
    break;
  case OptFlags::ALL:
    for (int row = 0; row < height; row++)
      decompressRow<OptFlags::ALL>(row);
    break;

  case OptFlags::SKIP:
    for (int row = 0; row < height; row++)
      decompressRow<OptFlags::SKIP>(row);
    break;
  case OptFlags::MV:
    for (int row = 0; row < height; row++)
      decompressRow<OptFlags::MV>(row);
    break;
  case OptFlags::QP:
    for (int row = 0; row < height; row++)
      decompressRow<OptFlags::QP>(row);
    break;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch"
  case OptFlags::SKIP | OptFlags::MV:
    for (int row = 0; row < height; row++)
      decompressRow<OptFlags::SKIP | OptFlags::MV>(row);
    break;
  case OptFlags::SKIP | OptFlags::QP:
    for (int row = 0; row < height; row++)
      decompressRow<OptFlags::SKIP | OptFlags::QP>(row);
    break;

  case OptFlags::MV | OptFlags::QP:
    for (int row = 0; row < height; row++)
      decompressRow<OptFlags::MV | OptFlags::QP>(row);
    break;
#pragma GCC diagnostic pop

  default:
    __builtin_unreachable();
  }
}

// The format is relatively straightforward. Each line gets encoded as a set
// of differences from pixels from another line. Pixels are grouped in blocks
// of 16 (8 green, 8 red or blue). Each block is encoded in three sections.
// First 1 or 4 bits to specify which reference pixels to use, then a section
// that specifies for each pixel the number of bits in the difference, then
// the actual difference bits

template <SamsungV2Decompressor::OptFlags optflags>
inline __attribute__((always_inline)) std::array<uint16_t, 16>
SamsungV2Decompressor::prepareBaselineValues(BitPumpMSB32* pump, int row,
                                             int col) {
  const Array2DRef<uint16_t> img(mRaw->getU16DataAsUncroppedArray2DRef());

  std::array<uint16_t, 16> baseline;

  if (!(optflags & OptFlags::QP) && !(col & 63)) {
    static constexpr std::array<int32_t, 3> scalevals = {{0, -2, 2}};
    uint32_t i = pump->getBits(2);
    scale = i < 3 ? scale + scalevals[i] : pump->getBits(12);
  }

  // First we figure out which reference pixels mode we're in
  if (optflags & OptFlags::MV)
    motion = pump->getBits(1) ? 3 : 7;
  else if (!pump->getBits(1))
    motion = pump->getBits(3);

  if ((row == 0 || row == 1) && (motion != 7))
    ThrowRDE("At start of image and motion isn't 7. File corrupted?");

  if (motion == 7) {
    // The base case.
    // If we're at the left edge we just start at the initial value.
    if (col == 0) {
      baseline.fill(initVal);
      return baseline;
    }
    // Else just set all pixels to the previous ones on the same line.
    std::array<int, 2> prev;
    for (int i = 0; i < 2; i++)
      prev[i] = img(row, col + i - 2);
    for (int i = 0; i < 16; i++)
      baseline[i] = prev[i & 1];
    return baseline;
  }

  // The complex case, we now need to actually lookup one or two lines above
  if (row < 2)
    ThrowRDE("Got a previous line lookup on first two lines. File corrupted?");

  static constexpr std::array<int32_t, 7> motionOffset = {-4, -2, -2, 0,
                                                          0,  2,  4};
  static constexpr std::array<int32_t, 7> motionDoAverage = {0, 0, 1, 0,
                                                             1, 0, 0};

  int32_t slideOffset = motionOffset[motion];
  int32_t doAverage = motionDoAverage[motion];

  for (int i = 0; i < 16; i++) {
    int refRow = row;
    int refCol = col + i + slideOffset;

    if ((row + i) & 1) { // Red or blue pixels use same color two lines up
      refRow -= 2;
    } else { // Green pixel N uses Green pixel N from row above
      refRow -= 1;
      refCol += (i & 1) ? -1 : 1; // (top left or top right)
    }

    if (refCol < 0)
      ThrowRDE("Bad motion %u at the beginning of the row", motion);
    if ((refCol >= width) || (doAverage && (refCol + 2 >= width)))
      ThrowRDE("Bad motion %u at the end of the row", motion);

    // In some cases we use as reference interpolation of this pixel and
    // the next
    if (doAverage) {
      baseline[i] = (img(refRow, refCol) + img(refRow, refCol + 2) + 1) >> 1;
    } else
      baseline[i] = img(refRow, refCol);
  }

  return baseline;
}

template <SamsungV2Decompressor::OptFlags optflags>
inline __attribute__((always_inline)) std::array<uint32_t, 4>
SamsungV2Decompressor::decodeDiffLengths(BitPumpMSB32* pump, int row) {
  if (!(optflags & OptFlags::SKIP || !pump->getBits(1)))
    return {};

  std::array<uint32_t, 4> diffBits;

  // Figure out how many difference bits we have to read for each pixel
  std::array<uint32_t, 4> flags;
  for (unsigned int& flag : flags)
    flag = pump->getBits(2);

  for (int i = 0; i < 4; i++) {
    // The color is 0-Green 1-Blue 2-Red
    uint32_t colornum = (row % 2 != 0) ? i >> 1 : ((i >> 1) + 2) % 3;

    assert(flags[i] <= 3);
    switch (flags[i]) {
    case 0:
      diffBits[i] = diffBitsMode[colornum][0];
      break;
    case 1:
      diffBits[i] = diffBitsMode[colornum][0] + 1;
      break;
    case 2:
      if (diffBitsMode[colornum][0] == 0)
        ThrowRDE("Difference bits underflow. File corrupted?");
      diffBits[i] = diffBitsMode[colornum][0] - 1;
      break;
    case 3:
      diffBits[i] = pump->getBits(4);
      break;
    default:
      __builtin_unreachable();
    }

    diffBitsMode[colornum][0] = diffBitsMode[colornum][1];
    diffBitsMode[colornum][1] = diffBits[i];

    if (diffBits[i] > bitDepth + 1)
      ThrowRDE("Too many difference bits (%u). File corrupted?", diffBits[i]);
    assert(diffBits[i] <= 15 && "So any difference fits within uint16_t");
  }

  return diffBits;
}

template <SamsungV2Decompressor::OptFlags optflags>
inline __attribute__((always_inline)) std::array<int, 16>
SamsungV2Decompressor::decodeDifferences(BitPumpMSB32* pump, int row) {
  // Figure out how many difference bits we have to read for each pixel
  const std::array<uint32_t, 4> diffBits =
      decodeDiffLengths<optflags>(pump, row);

  // Actually read the differences. We know these fit into 15-bit ints.
  std::array<int16_t, 16> diffs;
  for (int i = 0; i < 16; i++) {
    uint32_t len = diffBits[i >> 2];
    int16_t diff = getDiff(pump, len);
    diffs[i] = diff;
  }

  // Reshuffle the differences, while they still are only 16-bit.
  std::array<int16_t, 16> shuffled;
  for (int i = 0; i < 16; i++) {
    int p;
    // The differences are stored interlaced:
    // 0 2 4 6 8 10 12 14 1 3 5 7 9 11 13 15
    if (row % 2)
      p = ((i % 8) << 1) - (i >> 3) + 1;
    else
      p = ((i % 8) << 1) + (i >> 3);

    shuffled[p] = diffs[i];
  }

  // And finally widen and scale the differences.
  std::array<int, 16> scaled;
  for (int i = 0; i < 16; i++) {
    int scaledDiff = int(shuffled[i]) * (scale * 2 + 1) + scale;
    scaled[i] = scaledDiff;
  }

  return scaled;
}

template <SamsungV2Decompressor::OptFlags optflags>
void SamsungV2Decompressor::processBlock(BitPumpMSB32* pump, int row, int col) {
  const Array2DRef<uint16_t> out(mRaw->getU16DataAsUncroppedArray2DRef());

  const std::array<uint16_t, 16> baseline =
      prepareBaselineValues<optflags>(pump, row, col);

  // Figure out how many difference bits we have to read for each pixel
  const std::array<int, 16> diffs = decodeDifferences<optflags>(pump, row);

  // Actually apply the differences and write them to the pixels
  for (int i = 0; i < 16; ++i, ++col)
    out(row, col) = clampBits(baseline[i] + diffs[i], bits);
}

template <SamsungV2Decompressor::OptFlags optflags>
void SamsungV2Decompressor::decompressRow(int row) {

  // Align pump to 16byte boundary
  const auto line_offset = data.getPosition();
  if ((line_offset & 0xf) != 0)
    data.skipBytes(16 - (line_offset & 0xf));

  BitPumpMSB32 pump(data);

  // Initialize the motion and diff modes at the start of the line
  motion = 7;
  // By default we are not scaling values at all
  scale = 0;

  for (auto& i : diffBitsMode)
    i[0] = i[1] = (row == 0 || row == 1) ? 7 : 4;

  assert(width >= 16);
  assert(width % 16 == 0);
  for (int col = 0; col < width; col += 16)
    processBlock<optflags>(&pump, row, col);

  data.skipBytes(pump.getBufferPosition());
}

} // namespace rawspeed
