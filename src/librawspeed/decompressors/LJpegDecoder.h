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

#include "adt/Point.h"
#include "decompressors/AbstractLJpegDecoder.h"
#include <cstdint>

namespace rawspeed {

class ByteStream;
class RawImage;

// Decompresses Lossless JPEGs, with 2-4 components

class LJpegDecoder final : public AbstractLJpegDecoder {
  [[nodiscard]] ByteStream::size_type decodeScan() override;

  uint32_t offX = 0;
  uint32_t offY = 0;
  uint32_t w = 0;
  uint32_t h = 0;

  iPoint2D maxDim;

public:
  LJpegDecoder(ByteStream bs, const RawImage& img);

  void decode(uint32_t offsetX, uint32_t offsetY, uint32_t width,
              uint32_t height, iPoint2D maxDim, bool fixDng16Bug_);
};

} // namespace rawspeed
