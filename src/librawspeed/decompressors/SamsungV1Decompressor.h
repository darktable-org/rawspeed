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

#include "common/Common.h"                             // for int32
#include "decompressors/AbstractSamsungDecompressor.h" // for AbstractSamsu...
#include "io/BitPumpMSB.h"                             // for BitPumpMSB
#include <vector>                                      // for vector

namespace rawspeed {

class ByteStream;
class RawImage;
class TiffIFD;

// Decoder for compressed srw files (NX3000 and later)
class SamsungV1Decompressor final : public AbstractSamsungDecompressor {
  struct encTableItem;
  static int32 samsungDiff(BitPumpMSB* pump,
                           const std::vector<encTableItem>& tbl);

  const TiffIFD* raw;
  const ByteStream* bs;
  int bits;

public:
  SamsungV1Decompressor(const RawImage& image, const TiffIFD* ifd,
                        const ByteStream* bs_, int bit)
      : AbstractSamsungDecompressor(image), raw(ifd), bs(bs_), bits(bit) {}

  void decompress();
};

} // namespace rawspeed
