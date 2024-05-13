/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2014 Pedro Côrte-Real
    Copyright (C) 2016-2019 Roman Lebedev

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

#pragma once

#include "adt/Point.h"
#include "bitstreams/BitStreams.h"
#include "common/RawImage.h"
#include "decompressors/AbstractDecompressor.h"
#include "io/ByteStream.h"
#include "io/Endianness.h"
#include <cstdint>

namespace rawspeed {

class iPoint2D;

class UncompressedDecompressor final : public AbstractDecompressor {
  ByteStream input;
  RawImage mRaw;

  const iPoint2D size;
  const iPoint2D offset;
  int inputPitchBytes;
  int bitPerPixel;
  BitOrder order;

  uint32_t skipBytes;

  // check buffer size, throw, or compute minimal height that can be decoded
  void sanityCheck(const uint32_t* h, int bytesPerLine) const;

  // check buffer size, throw, or compute minimal height that can be decoded
  void sanityCheck(uint32_t w, const uint32_t* h, int bpp) const;

  // for special packed formats
  static int bytesPerLine(int w, bool skips);

  template <typename Pump, typename NarrowFpType>
  void decodePackedFP(int rows, int row) const;

  template <typename Pump> void decodePackedInt(int rows, int row) const;

public:
  UncompressedDecompressor(ByteStream input, RawImage img,
                           const iRectangle2D& crop, int inputPitchBytes,
                           int bitPerPixel, BitOrder order);

  /* Helper function for decoders, that will unpack uncompressed image data */
  /* input: Input image, positioned at first pixel */
  /* size: Size of the image to decode in pixels */
  /* offset: offset to write the data into the final image */
  /* inputPitch: Number of bytes between each line in the input image */
  /* bitPerPixel: Number of bits to read for each input pixel. */
  /* order: Order of the bits - see Common.h for possibilities. */
  void readUncompressedRaw();

  /* Faster versions for unpacking 8 bit data */
  template <bool uncorrectedRawValues> void decode8BitRaw();

  /* Faster version for unpacking 12 bit data with control byte every 10 pixels
   */
  template <Endianness e> void decode12BitRawWithControl();

  /* Faster version for reading unpacked 12 bit data that is left aligned
   * (needs >> 4 shift) */
  template <Endianness e> void decode12BitRawUnpackedLeftAligned();
};

extern template void UncompressedDecompressor::decode8BitRaw<false>();
extern template void UncompressedDecompressor::decode8BitRaw<true>();

extern template void
UncompressedDecompressor::decode12BitRawWithControl<Endianness::little>();
extern template void
UncompressedDecompressor::decode12BitRawWithControl<Endianness::big>();

extern template void
UncompressedDecompressor::decode12BitRawUnpackedLeftAligned<
    Endianness::little>();
extern template void
UncompressedDecompressor::decode12BitRawUnpackedLeftAligned<Endianness::big>();

} // namespace rawspeed
