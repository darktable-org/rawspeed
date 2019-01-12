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

#include "rawspeedconfig.h" // for HAVE_ZLIB

#ifdef HAVE_ZLIB

#include "common/Common.h"                      // for uint32
#include "common/Point.h"                       // for iPoint2D
#include "common/RawImage.h"                    // for RawImage
#include "decompressors/AbstractDecompressor.h" // for AbstractDecompressor
#include "io/ByteStream.h"                      // for ByteStream
#include <memory>                               // for unique_ptr
#include <utility>                              // for move

namespace rawspeed {

class DeflateDecompressor final : public AbstractDecompressor {
  ByteStream input;
  RawImage mRaw;
  int predictor;
  int bps;

public:
  DeflateDecompressor(ByteStream bs, const RawImage& img, int predictor_,
                      int bps_)
      : input(std::move(bs)), mRaw(img), predictor(predictor_), bps(bps_) {}

  void decode(std::unique_ptr<unsigned char[]>* uBuffer, // NOLINT
              iPoint2D maxDim, iPoint2D dim, iPoint2D off);
};

} // namespace rawspeed

#else

#pragma message                                                                \
    "ZLIB is not present! Deflate compression will not be supported!"

#endif
