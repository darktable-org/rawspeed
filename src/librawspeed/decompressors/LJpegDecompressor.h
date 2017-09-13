/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2017 Axel Waggershauser

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

#include "common/Common.h"                           // for uint32
#include "decompressors/AbstractLJpegDecompressor.h" // for AbstractLJpegDe...
#include "io/Buffer.h"                               // for Buffer, Buffer:...
#include "io/ByteStream.h"                           // for ByteStream

namespace rawspeed {

class RawImage;

// Decompresses Lossless JPEGs, with 2-4 components

class LJpegDecompressor final : public AbstractLJpegDecompressor
{
  void decodeScan() override;
  template<int N_COMP> void decodeN();

  uint32 offX = 0;
  uint32 offY = 0;

public:
  LJpegDecompressor(const ByteStream& bs, const RawImage& img)
      : AbstractLJpegDecompressor(bs, img) {}

  void decode(uint32 offsetX, uint32 offsetY, bool fixDng16Bug);
};

} // namespace rawspeed
