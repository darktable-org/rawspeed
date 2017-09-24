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
#include "common/Common.h"                // for uint32, ushort16, int32
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

enum struct SamsungV2Decompressor::OptFlags : uint32 {
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

SamsungV2Decompressor::SamsungV2Decompressor(const RawImage& image,
                                             const ByteStream& bs, int bit)
    : AbstractSamsungDecompressor(image), bits(bit) {
  BitPumpMSB32 startpump(bs);

  // Process the initial metadata bits, we only really use initVal, width and
  // height (the last two match the TIFF values anyway)
  startpump.getBits(16); // NLCVersion
  startpump.getBits(4);  // ImgFormat
  bitDepth = startpump.getBits(4) + 1;
  startpump.getBits(4); // NumBlkInRCUnit
  startpump.getBits(4); // CompressionRatio
  width = startpump.getBits(16);
  height = startpump.getBits(16);
  startpump.getBits(16); // TileWidth
  startpump.getBits(4);  // reserved

  // The format includes an optimization code that sets 3 flags to change the
  // decoding parameters
  const uint32 optflags = startpump.getBits(4);
  if (optflags > static_cast<uint32>(OptFlags::ALL))
    ThrowRDE("Invalid opt flags %x", optflags);
  _flags = static_cast<OptFlags>(optflags);

  startpump.getBits(8); // OverlapWidth
  startpump.getBits(8); // reserved
  startpump.getBits(8); // Inc
  startpump.getBits(2); // reserved
  initVal = startpump.getBits(14);

  if (width == 0 || height == 0 || width % 16 != 0 || width > 6496 ||
      height > 4336)
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", width, height);

  if (width != static_cast<uint32>(mRaw->dim.x) ||
      height != static_cast<uint32>(mRaw->dim.y))
    ThrowRDE("EXIF image dimensions do not match dimensions from raw header");

  data = startpump.getStream(startpump.getRemainSize());
}

void SamsungV2Decompressor::decompress() {
  switch (_flags) {
  case OptFlags::NONE:
    for (uint32 row = 0; row < height; row++)
      decompressRow<OptFlags::NONE>(row);
    break;
  case OptFlags::ALL:
    for (uint32 row = 0; row < height; row++)
      decompressRow<OptFlags::ALL>(row);
    break;

  case OptFlags::SKIP:
    for (uint32 row = 0; row < height; row++)
      decompressRow<OptFlags::SKIP>(row);
    break;
  case OptFlags::MV:
    for (uint32 row = 0; row < height; row++)
      decompressRow<OptFlags::MV>(row);
    break;
  case OptFlags::QP:
    for (uint32 row = 0; row < height; row++)
      decompressRow<OptFlags::QP>(row);
    break;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch"
  case OptFlags::SKIP | OptFlags::MV:
    for (uint32 row = 0; row < height; row++)
      decompressRow<OptFlags::SKIP | OptFlags::MV>(row);
    break;
  case OptFlags::SKIP | OptFlags::QP:
    for (uint32 row = 0; row < height; row++)
      decompressRow<OptFlags::SKIP | OptFlags::QP>(row);
    break;

  case OptFlags::MV | OptFlags::QP:
    for (uint32 row = 0; row < height; row++)
      decompressRow<OptFlags::MV | OptFlags::QP>(row);
    break;
#pragma GCC diagnostic pop

  default:
    __builtin_unreachable();
  }
}

template <SamsungV2Decompressor::OptFlags optflags>
void SamsungV2Decompressor::decompressRow(uint32 row) {
  // The format is relatively straightforward. Each line gets encoded as a set
  // of differences from pixels from another line. Pixels are grouped in blocks
  // of 16 (8 green, 8 red or blue). Each block is encoded in three sections.
  // First 1 or 4 bits to specify which reference pixels to use, then a section
  // that specifies for each pixel the number of bits in the difference, then
  // the actual difference bits

  // Align pump to 16byte boundary
  const auto line_offset = data.getPosition();
  if ((line_offset & 0xf) != 0)
    data.skipBytes(16 - (line_offset & 0xf));

  BitPumpMSB32 pump(data);

  auto* img = reinterpret_cast<ushort16*>(mRaw->getData(0, row));
  ushort16* img_up = reinterpret_cast<ushort16*>(
      mRaw->getData(0, std::max(0, static_cast<int>(row) - 1)));
  ushort16* img_up2 = reinterpret_cast<ushort16*>(
      mRaw->getData(0, std::max(0, static_cast<int>(row) - 2)));

  // Initialize the motion and diff modes at the start of the line
  uint32 motion = 7;
  // By default we are not scaling values at all
  int32 scale = 0;

  uint32 diffBitsMode[3][2] = {{0}};
  for (auto& i : diffBitsMode)
    i[0] = i[1] = (row == 0 || row == 1) ? 7 : 4;

  assert(width >= 16);
  for (uint32 col = 0; col < width; col += 16) {
    if (!(optflags & OptFlags::QP) && !(col & 63)) {
      int32 scalevals[] = {0, -2, 2};
      uint32 i = pump.getBits(2);
      scale = i < 3 ? scale + scalevals[i] : pump.getBits(12);
    }

    // First we figure out which reference pixels mode we're in
    if (optflags & OptFlags::MV)
      motion = pump.getBits(1) ? 3 : 7;
    else if (!pump.getBits(1))
      motion = pump.getBits(3);

    if ((row == 0 || row == 1) && (motion != 7))
      ThrowRDE("At start of image and motion isn't 7. File corrupted?");

    if (motion == 7) {
      // The base case, just set all pixels to the previous ones on the same
      // line If we're at the left edge we just start at the initial value
      for (uint32 i = 0; i < 16; i++)
        img[i] = (col == 0) ? initVal : *(img + i - 2);
    } else {
      // The complex case, we now need to actually lookup one or two lines
      // above
      if (row < 2)
        ThrowRDE(
            "Got a previous line lookup on first two lines. File corrupted?");

      int32 motionOffset[7] = {-4, -2, -2, 0, 0, 2, 4};
      int32 motionDoAverage[7] = {0, 0, 1, 0, 1, 0, 0};

      int32 slideOffset = motionOffset[motion];
      int32 doAverage = motionDoAverage[motion];

      for (uint32 i = 0; i < 16; i++) {
        ushort16* refpixel;

        if ((row + i) & 0x1) {
          // Red or blue pixels use same color two lines up
          refpixel = img_up2 + i + slideOffset;

          if (col == 0 && img_up2 > refpixel)
            ThrowRDE("Bad motion %u at the beginning of the row", motion);
        } else {
          // Green pixel N uses Green pixel N from row above
          // (top left or top right)
          refpixel = img_up + i + slideOffset + (((i % 2) != 0) ? -1 : 1);

          if (col == 0 && img_up > refpixel)
            ThrowRDE("Bad motion %u at the beginning of the row", motion);
        }

        // In some cases we use as reference interpolation of this pixel and
        // the next
        if (doAverage)
          img[i] = (*refpixel + *(refpixel + 2) + 1) >> 1;
        else
          img[i] = *refpixel;
      }
    }

    // Figure out how many difference bits we have to read for each pixel
    uint32 diffBits[4] = {0};
    if (optflags & OptFlags::SKIP || !pump.getBits(1)) {
      uint32 flags[4];
      for (unsigned int& flag : flags)
        flag = pump.getBits(2);

      for (uint32 i = 0; i < 4; i++) {
        // The color is 0-Green 1-Blue 2-Red
        uint32 colornum = (row % 2 != 0) ? i >> 1 : ((i >> 1) + 2) % 3;

        assert(flags[i] <= 3);
        switch (flags[i]) {
        case 0:
          diffBits[i] = diffBitsMode[colornum][0];
          break;
        case 1:
          diffBits[i] = diffBitsMode[colornum][0] + 1;
          break;
        case 2:
          diffBits[i] = diffBitsMode[colornum][0] - 1;
          break;
        case 3:
          diffBits[i] = pump.getBits(4);
          break;
        default:
          __builtin_unreachable();
        }

        diffBitsMode[colornum][0] = diffBitsMode[colornum][1];
        diffBitsMode[colornum][1] = diffBits[i];

        if (diffBits[i] > bitDepth + 1)
          ThrowRDE("Too many difference bits. File corrupted?");
      }
    }

    // Actually read the differences and write them to the pixels
    for (uint32 i = 0; i < 16; i++) {
      uint32 len = diffBits[i >> 2];
      int32 diff = pump.getBits(len);

      // If the first bit is 1 we need to turn this into a negative number
      if (len != 0 && diff >> (len - 1))
        diff -= (1 << len);

      ushort16* value = nullptr;
      // Apply the diff to pixels 0 2 4 6 8 10 12 14 1 3 5 7 9 11 13 15
      if (row % 2)
        value = &img[((i & 0x7) << 1) + 1 - (i >> 3)];
      else
        value = &img[((i & 0x7) << 1) + (i >> 3)];

      diff = diff * (scale * 2 + 1) + scale;
      *value = clampBits(static_cast<int>(*value) + diff, bits);
    }

    img += 16;
    img_up += 16;
    img_up2 += 16;
  }

  data.skipBytes(pump.getBufferPosition());
}

} // namespace rawspeed
