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

#include "common/RawImage.h"
#include "decompressors/AbstractDecompressor.h"
#include "io/Buffer.h"
#include <memory>

namespace rawspeed {

class iPoint2D;

class DeflateDecompressor final : public AbstractDecompressor {
  Buffer input;
  RawImage mRaw;
  int predFactor;
  int bps;

public:
  DeflateDecompressor(Buffer bs, RawImage img, int predictor, int bps_);

  // NOLINTNEXTLINE(modernize-avoid-c-arrays)
  void decode(std::unique_ptr<unsigned char[]>* uBuffer, iPoint2D maxDim,
              iPoint2D dim, iPoint2D off);
};

} // namespace rawspeed

#else

#pragma message                                                                \
    "ZLIB is not present! Deflate compression will not be supported!"

#endif
