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

#include "common/RawImage.h"
#include "decompressors/AbstractDecompressor.h"
#include "io/Buffer.h"
#include <cstdint>
#include <utility>

namespace rawspeed {

class JpegDecompressor final : public AbstractDecompressor {
  struct JpegDecompressStruct;

  Buffer input;
  RawImage mRaw;

public:
  JpegDecompressor(Buffer bs, RawImage img) : input(bs), mRaw(std::move(img)) {}

  void decode(uint32_t offsetX, uint32_t offsetY);
};

} // namespace rawspeed

#else

#pragma message                                                                \
    "JPEG is not present! Lossy JPEG compression will not be supported!"

#endif
