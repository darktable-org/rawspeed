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

#include "bitstreams/BitStreamerMSB.h"
#include "common/RawImage.h"
#include "decompressors/AbstractDecompressor.h"
#include <cstdint>

namespace rawspeed {

class ByteStream;

class SonyArw1Decompressor final : public AbstractDecompressor {
  RawImage mRaw;

  inline static int getDiff(BitStreamerMSB& bs, uint32_t len);

public:
  explicit SonyArw1Decompressor(RawImage img);
  void decompress(ByteStream input) const;
};

} // namespace rawspeed
