/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2026 darktable developers

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

#ifdef HAVE_JPEGXL

#include "common/RawImage.h"
#include "decompressors/AbstractDecompressor.h"
#include "io/Buffer.h"
#include <cstdint>
#include <utility>

namespace rawspeed {

// Decodes a single DNG tile whose data is a self-contained JPEG XL codestream
// (TIFF Compression tag 52546, as used by Apple ProRAW on iPhone 16 / DNG 1.7).
class JpegXlDecompressor final : public AbstractDecompressor {
  Buffer input;
  RawImage mRaw;
  // The DNG's BitsPerSample for this IFD. The decoded codestream must agree
  // with it, because that is the depth the BlackLevel/WhiteLevel are keyed to.
  uint32_t dngBps;

public:
  JpegXlDecompressor(Buffer bs, RawImage img, uint32_t dngBps_)
      : input(bs), mRaw(std::move(img)), dngBps(dngBps_) {}

  void decode(uint32_t offsetX, uint32_t offsetY);
};

} // namespace rawspeed

#else

#pragma message                                                                \
    "JPEG XL is not present! DNG JPEG XL (DNG 1.7) compression will not be "   \
    "supported!"

#endif
