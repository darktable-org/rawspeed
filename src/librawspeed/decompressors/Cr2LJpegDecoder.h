/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2017 Axel Waggershauser
    Copyright (C) 2018 Roman Lebedev

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

#include "decompressors/AbstractLJpegDecompressor.h" // for AbstractLJpegDe...
#include "decompressors/Cr2Decompressor.h"           // for Cr2Decompressor
#include <cassert>                                   // for assert
#include <cstdint>                                   // for uint16_t

namespace rawspeed {

class ByteStream;
class RawImage;

class Cr2LJpegDecoder final : public AbstractLJpegDecompressor
{
  Cr2Slicing slicing;

  void decodeScan() override;

public:
  Cr2LJpegDecoder(const ByteStream& bs, const RawImage& img);
  void decode(const Cr2Slicing& slicing);
};

} // namespace rawspeed
