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

#ifdef HAVE_ZLIB

#include "common/Common.h"   // for getHostEndianness, uint32, Endianness::big
#include "common/RawImage.h" // for RawImage
#include "decompressors/AbstractDecompressor.h" // for AbstractDecompressor
#include "io/Buffer.h"                          // for Buffer, Buffer::size_type
#include "io/ByteStream.h"                      // for ByteStream

namespace rawspeed {

class DeflateDecompressor final : public AbstractDecompressor {
public:
  DeflateDecompressor(const Buffer& data, Buffer::size_type offset,
                      Buffer::size_type size, const RawImage& img,
                      int predictor_, int bps_)
      : input(data, offset, size), mRaw(img), predictor(predictor_), bps(bps_) {
  }

  void decode(unsigned char** uBuffer, int width, int height, uint32 offX,
              uint32 offY);

protected:
  ByteStream input;
  RawImage mRaw;
  int predictor;
  int bps;
};

} // namespace rawspeed

#else

#pragma message                                                                \
    "ZLIB is not present! Deflate compression will not be supported!"

#endif
