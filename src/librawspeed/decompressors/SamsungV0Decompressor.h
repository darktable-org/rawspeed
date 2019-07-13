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

#include "common/Common.h"                             // for int32_t, uint32_t
#include "decompressors/AbstractSamsungDecompressor.h" // for AbstractSamsu...
#include "io/BitPumpMSB32.h"                           // for BitPumpMSB32
#include "io/ByteStream.h"                             // for ByteStream
#include <vector>                                      // for vector

namespace rawspeed {

class RawImage;

// Decoder for compressed srw files (NX300 and later)
class SamsungV0Decompressor final : public AbstractSamsungDecompressor {
  std::vector<ByteStream> stripes;

  void computeStripes(ByteStream bso, ByteStream bsr);

  void decompressStrip(uint32_t y, const ByteStream& bs) const;

  static int32_t calcAdj(BitPumpMSB32* bits, int b);

public:
  SamsungV0Decompressor(const RawImage& image, const ByteStream& bso,
                        const ByteStream& bsr);

  void decompress() const;
};

} // namespace rawspeed
