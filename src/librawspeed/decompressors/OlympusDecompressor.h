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

#include "common/Common.h"                      // for extractHighBits
#include "common/RawImage.h"                    // for RawImage
#include "common/SimpleLUT.h"                   // for SimpleLUT<>::value_type
#include "decompressors/AbstractDecompressor.h" // for AbstractDecompressor
#include "io/BitPumpMSB.h"                      // for BitPumpMSB
#include <algorithm>                            // for min
#include <array>                                // for array
#include <cstdint>                              // for uint16_t

namespace rawspeed {

class ByteStream;

template <class T> class Array2DRef;

class OlympusDecompressor final : public AbstractDecompressor {
  RawImageData* mRaw;

  // A table to quickly look up "high" value
  const SimpleLUT<char, 12> bittable{
      [](unsigned i, [[maybe_unused]] unsigned tableSize) {
        int high;
        for (high = 0; high < 12; high++)
          if (extractHighBits(i, high, /*effectiveBitwidth=*/11) & 1)
            break;
        return std::min(12, high);
      }};

  inline __attribute__((always_inline)) int
  parseCarry(BitPumpMSB& bits, std::array<int, 3>* carry) const;

  static inline int getPred(Array2DRef<uint16_t> out, int row, int col);

  void decompressRow(BitPumpMSB& bits, int row) const;

public:
  explicit OlympusDecompressor(RawImageData* img);
  void decompress(ByteStream input) const;
};

} // namespace rawspeed
