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

#include "common/RawImage.h"                    // for RawImage
#include "common/SimpleLUT.h"                   // for SimpleLUT
#include "decompressors/AbstractDecompressor.h" // for AbstractDecompressor
#include "io/BitPumpMSB.h"                      // for BitPumpMSB
#include <array>                                // for array, array<>::value...

namespace rawspeed {

class ByteStream;

class OlympusDecompressor final : public AbstractDecompressor {
  RawImage mRaw;

  // A table to quickly look up "high" value
  const SimpleLUT<char, 12> bittable{[](unsigned i, unsigned tableSize) {
    int b = i;
    int high;
    for (high = 0; high < 12; high++)
      if ((b >> (11 - high)) & 1)
        break;
    return std::min(12, high);
  }};

  inline __attribute__((always_inline)) int
  parseCarry(BitPumpMSB* bits, std::array<int, 3>* carry) const;

  static inline int getPred(int row, int x, uint16_t* dest,
                            const uint16_t* up_ptr);

  void decompressRow(BitPumpMSB* bits, int row) const;

public:
  explicit OlympusDecompressor(const RawImage& img);
  void decompress(ByteStream input) const;
};

} // namespace rawspeed
