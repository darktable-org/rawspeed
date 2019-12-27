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

#include "common/Common.h"                      // for BitOrder
#include "common/RawImage.h"                    // for RawImage
#include "decompressors/AbstractDecompressor.h" // for AbstractDecompressor
#include "io/ByteStream.h"                      // for ByteStream
#include "io/Endianness.h"                      // for Endianness
#include <cstdint>                              // for uint32_t
#include <utility>                              // for move

namespace rawspeed {

class iPoint2D;

class UncompressedDecompressor final : public AbstractDecompressor {
  ByteStream input;
  RawImage mRaw;

  // check buffer size, throw, or compute minimal height that can be decoded
  void sanityCheck(const uint32_t* h, int bytesPerLine);

  // check buffer size, throw, or compute minimal height that can be decoded
  void sanityCheck(uint32_t w, const uint32_t* h, int bpp);

  // for special packed formats
  static int bytesPerLine(int w, bool skips);

public:
  UncompressedDecompressor(ByteStream input_, const RawImage& img)
      : input(std::move(input_)), mRaw(img) {}

  /* Helper function for decoders, that will unpack uncompressed image data */
  /* input: Input image, positioned at first pixel */
  /* size: Size of the image to decode in pixels */
  /* offset: offset to write the data into the final image */
  /* inputPitch: Number of bytes between each line in the input image */
  /* bitPerPixel: Number of bits to read for each input pixel. */
  /* order: Order of the bits - see Common.h for possibilities. */
  void readUncompressedRaw(const iPoint2D& size, const iPoint2D& offset,
                           int inputPitchBytes, int bitPerPixel,
                           BitOrder order);

  /* Faster versions for unpacking 8 bit data */
  template <bool uncorrectedRawValues>
  void decode8BitRaw(uint32_t w, uint32_t h);

  /* Faster version for unpacking 12 bit data */
  /* interlaced - is data with interlaced lines ? */
  /* skips - is there control byte every 10 pixels ? */
  template <Endianness e, bool interlaced = false, bool skips = false>
  void decode12BitRaw(uint32_t w, uint32_t h);

  /* Faster version for reading unpacked 12 bit data that is left aligned
   * (needs >> 4 shift) */
  template <Endianness e>
  void decode12BitRawUnpackedLeftAligned(uint32_t w, uint32_t h);

  /* Faster version for reading unpacked data */
  template <int bits, Endianness e>
  void decodeRawUnpacked(uint32_t w, uint32_t h);
};

extern template void UncompressedDecompressor::decode8BitRaw<false>(uint32_t w,
                                                                    uint32_t h);
extern template void UncompressedDecompressor::decode8BitRaw<true>(uint32_t w,
                                                                   uint32_t h);

extern template void
UncompressedDecompressor::decode12BitRaw<Endianness::little, false, false>(
    uint32_t w, uint32_t h);
extern template void
UncompressedDecompressor::decode12BitRaw<Endianness::big, false, false>(
    uint32_t w, uint32_t h);
extern template void
UncompressedDecompressor::decode12BitRaw<Endianness::big, true, false>(
    uint32_t w, uint32_t h);
extern template void
UncompressedDecompressor::decode12BitRaw<Endianness::little, false, true>(
    uint32_t w, uint32_t h);
extern template void
UncompressedDecompressor::decode12BitRaw<Endianness::big, false, true>(
    uint32_t w, uint32_t h);

extern template void
UncompressedDecompressor::decode12BitRawUnpackedLeftAligned<Endianness::big>(
    uint32_t w, uint32_t h);

extern template void
UncompressedDecompressor::decodeRawUnpacked<12, Endianness::little>(uint32_t w,
                                                                    uint32_t h);
extern template void
UncompressedDecompressor::decodeRawUnpacked<12, Endianness::big>(uint32_t w,
                                                                 uint32_t h);
extern template void
UncompressedDecompressor::decodeRawUnpacked<14, Endianness::big>(uint32_t w,
                                                                 uint32_t h);
extern template void
UncompressedDecompressor::decodeRawUnpacked<16, Endianness::little>(uint32_t w,
                                                                    uint32_t h);
extern template void
UncompressedDecompressor::decodeRawUnpacked<16, Endianness::big>(uint32_t w,
                                                                 uint32_t h);

} // namespace rawspeed
