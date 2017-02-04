/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
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

#include "common/RawImage.h"                         // for RawImage
#include "decompressors/AbstractLJpegDecompressor.h" // for AbstractLJpegDe...
#include "io/Buffer.h"                               // for Buffer, Buffer:...

namespace RawSpeed {

class HasselbladDecompressor final : public AbstractLJpegDecompressor
{
  int pixelBaseOffset = 0;

  void decodeScan() override;

public:
  HasselbladDecompressor(const Buffer& data, Buffer::size_type offset,
                         Buffer::size_type size, const RawImage& img)
      : AbstractLJpegDecompressor(data, offset, size, img) {}
  HasselbladDecompressor(const Buffer& data, Buffer::size_type offset,
                         const RawImage& img)
      : AbstractLJpegDecompressor(data, offset, img) {}

  void decode(int pixelBaseOffset_);
};

} // namespace RawSpeed
