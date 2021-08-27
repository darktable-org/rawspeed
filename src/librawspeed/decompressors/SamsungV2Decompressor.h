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
#include "io/BitPumpMSB32.h"                           // for BitPumpMSB32
#include "io/ByteStream.h"                             // for ByteStream
#include <array>                                       // for array
#include <cstdint>                                     // for uint32_t, uin...

namespace rawspeed {

class RawImage;

// Decoder for third generation compressed SRW files (NX1)
class SamsungV2Decompressor final : public AbstractSamsungDecompressor {
public:
  enum struct OptFlags : uint32_t;

protected:
  uint32_t bitDepth;
  int width;
  int height;
  OptFlags optflags;
  uint16_t initVal;

  ByteStream data;

  int motion;
  int scale;
  std::array<std::array<int, 2>, 3> diffBitsMode;

  static inline __attribute__((always_inline)) int16_t
  getDiff(BitPumpMSB32& pump, uint32_t len);

  inline __attribute__((always_inline)) std::array<uint16_t, 16>
  prepareBaselineValues(BitPumpMSB32& pump, int row, int col);

  inline __attribute__((always_inline)) std::array<uint32_t, 4>
  decodeDiffLengths(BitPumpMSB32& pump, int row);

  inline __attribute__((always_inline)) std::array<int, 16>
  decodeDifferences(BitPumpMSB32& pump, int row);

  inline __attribute__((always_inline)) void processBlock(BitPumpMSB32& pump,
                                                          int row, int col);

  void decompressRow(int row);

public:
  SamsungV2Decompressor(const RawImage& image, const ByteStream& bs,
                        unsigned bit);

  void decompress();
};

} // namespace rawspeed
