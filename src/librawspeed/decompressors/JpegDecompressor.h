/*
    RawSpeed - RAW file decoder.

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

#pragma once

#include "rawspeedconfig.h"

#ifdef HAVE_JPEG

#include "common/Common.h"                      // for uint32
#include "common/RawImage.h"                    // for RawImage
#include "decompressors/AbstractDecompressor.h" // for AbstractDecompressor
#include "io/Buffer.h"                          // for Buffer, Buffer::size_type
#include "io/ByteStream.h"                      // for ByteStream
#include "io/Endianness.h" // for getHostEndianness, Endianness::big

namespace rawspeed {

class JpegDecompressor final : public AbstractDecompressor {
public:
  JpegDecompressor(const Buffer& data, Buffer::size_type offset,
                   Buffer::size_type size, const RawImage& img)
      : input(data, offset, size, getHostEndianness() == big), mRaw(img) {}
  JpegDecompressor(const Buffer& data, Buffer::size_type offset,
                   const RawImage& img)
      : JpegDecompressor(data, offset, data.getSize() - offset, img) {}
  virtual ~JpegDecompressor() = default;

  void decode(uint32 offsetX, uint32 offsetY);

protected:
  struct JpegDecompressStruct;
  ByteStream input;
  RawImage mRaw;
};

} // namespace rawspeed

#else

#pragma message                                                                \
    "JPEG is not present! Lossy JPEG compression will not be supported!"

#endif
