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

#include "decompressors/AbstractSamsungDecompressor.h" // for AbstractSamsu...

namespace rawspeed {

class Buffer;
class RawImage;
class TiffIFD;

// Decoder for third generation compressed SRW files (NX1)
class SamsungV2Decompressor final : public AbstractSamsungDecompressor {
  const TiffIFD* raw;
  const Buffer* mFile;
  int bits;

public:
  SamsungV2Decompressor(const RawImage& image, const TiffIFD* ifd,
                        const Buffer* file, int bit)
      : AbstractSamsungDecompressor(image), raw(ifd), mFile(file), bits(bit) {}

  void decompress();
};

} // namespace rawspeed
