/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2017-2019 Roman Lebedev

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

#include "common/RawImage.h"
#include "decompressors/AbstractDecompressor.h"
#include "io/ByteStream.h"

namespace rawspeed {

class SonyArw2Decompressor final : public AbstractDecompressor {
  void decompressRow(int row) const;
  void decompressThread() const noexcept;

  RawImage mRaw;
  ByteStream input;

public:
  SonyArw2Decompressor(RawImage img, ByteStream input);
  void decompress() const;
};

} // namespace rawspeed
