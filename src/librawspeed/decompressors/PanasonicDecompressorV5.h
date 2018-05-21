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

#include "common/Common.h"                                  // for uint32
#include "decompressors/AbstractParallelizedDecompressor.h" // for Abstract...
#include "io/ByteStream.h"                                  // for ByteStream

namespace rawspeed {

class RawImage;

class PanasonicDecompressorV5 final : public AbstractParallelizedDecompressor {
  static constexpr uint32 BufSize = 0x4000;
  struct PanaBitpump;

  void decompressThreaded(const RawDecompressorThread* t) const final;

  ByteStream input;
  bool zero_is_bad;

  uint32 bps;

public:
  PanasonicDecompressorV5(const RawImage& img, const ByteStream& input_,
                        bool zero_is_not_bad, uint32 bps_);
};

} // namespace rawspeed
