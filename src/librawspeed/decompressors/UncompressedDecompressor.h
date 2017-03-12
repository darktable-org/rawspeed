/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2014 Pedro Côrte-Real
    Copyright (C) 2016-2017 Roman Lebedev

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

#include "common/Common.h"                      // for uint32, BitOrder
#include "common/RawImage.h"                    // for RawImage
#include "decompressors/AbstractDecompressor.h" // for AbstractDecompressor
#include "io/Buffer.h"                          // for Buffer, Buffer::size_type
#include "io/ByteStream.h"                      // for ByteStream
#include "io/Endianness.h"                      // for Endianness
#include <algorithm>                            // for move

namespace RawSpeed {

class iPoint2D;

class UncompressedDecompressor final : public AbstractDecompressor {
  // check buffer size, throw, or compute minimal height that can be decoded
  void sanityCheck(uint32* h, int bpl);

  // check buffer size, throw, or compute minimal height that can be decoded
  void sanityCheck(uint32 w, uint32* h, int bpp);

  // for special packed formats
  int bytesPerLine(int w, bool skips);

  template <Endianness e> void decode12BitRaw(uint32 w, uint32 h);
  template <Endianness e> void decode12BitRawWithControl(uint32 w, uint32 h);
  template <Endianness e> void decode12BitRawUnpacked(uint32 w, uint32 h);
  template <Endianness e> void decode14BitRawUnpacked(uint32 w, uint32 h);
  template <Endianness e> void decode16BitRawUnpacked(uint32 w, uint32 h);

public:
  UncompressedDecompressor(ByteStream input_, const RawImage& img,
                           bool uncorrectedRawValues_)
      : input(std::move(input_)), mRaw(img),
        uncorrectedRawValues(uncorrectedRawValues_) {}

  UncompressedDecompressor(const Buffer& data, Buffer::size_type offset,
                           Buffer::size_type size, const RawImage& img,
                           bool uncorrectedRawValues_)
      : UncompressedDecompressor(ByteStream(data, offset, size), img,
                                 uncorrectedRawValues_) {}

  UncompressedDecompressor(const Buffer& data, Buffer::size_type offset,
                           const RawImage& img, bool uncorrectedRawValues_)
      : UncompressedDecompressor(data, offset, data.getSize() - offset, img,
                                 uncorrectedRawValues_) {}

  /* Helper function for decoders, that will unpack uncompressed image data */
  /* input: Input image, positioned at first pixel */
  /* size: Size of the image to decode in pixels */
  /* offset: offset to write the data into the final image */
  /* inputPitch: Number of bytes between each line in the input image */
  /* bitPerPixel: Number of bits to read for each input pixel. */
  /* order: Order of the bits - see Common.h for possibilities. */
  void readUncompressedRaw(iPoint2D& size, iPoint2D& offset, int inputPitch,
                           int bitPerPixel, BitOrder order);

  /* Faster versions for unpacking 8 bit data */
  void decode8BitRaw(uint32 w, uint32 h);

  /* Faster version for unpacking 12 bit data */
  void decode12BitRaw(uint32 w, uint32 h, Endianness e);

  /* Faster version for unpacking 12 bit data with a control byte every 10 *
   * pixels */
  void decode12BitRawWithControl(uint32 w, uint32 h, Endianness e);

  /* Faster version for unpacking 12 bit MSB data with interlaced lines */
  void decode12BitRawBEInterlaced(uint32 w, uint32 h);

  /* Faster version for reading unpacked 12 bit MSB data that is left aligned
   * (needs >> 4 shift) */
  void decode12BitRawBEunpackedLeftAligned(uint32 w, uint32 h);

  /* Faster version for reading unpacked 12 bit data */
  void decode12BitRawUnpacked(uint32 w, uint32 h, Endianness e);

  /* Faster version for reading unpacked 14 bit data */
  void decode14BitRawUnpacked(uint32 w, uint32 h, Endianness e);

  /* Faster version for reading unpacked 16 bit data */
  void decode16BitRawUnpacked(uint32 w, uint32 h, Endianness e);

protected:
  ByteStream input;
  RawImage mRaw;
  bool uncorrectedRawValues;
};

} // namespace RawSpeed
