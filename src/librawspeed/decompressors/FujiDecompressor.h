/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2017 Uwe Müssel
    Copyright (C) 2017 Roman Lebedev

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#pragma once

#include "adt/Array2DRef.h"                     // for Array2DRef
#include "adt/Point.h"                          // for iPoint2D
#include "common/RawImage.h"                    // for RawImage
#include "decompressors/AbstractDecompressor.h" // for AbstractDecompressor
#include "io/BitPumpMSB.h"                      // for BitPumpMSB
#include "io/ByteStream.h"                      // for ByteStream
#include "metadata/ColorFilterArray.h"          // for CFAColor
#include <array>                                // for array
#include <cassert>                              // for assert
#include <cstdint>                              // for uint16_t, int8_t
#include <utility>                              // for pair
#include <vector>                               // for vector

namespace rawspeed {

namespace {

struct fuji_compressed_block;

}

class FujiDecompressor final : public AbstractDecompressor {
  RawImage mRaw;

  void decompressThread() const noexcept;

  friend fuji_compressed_block;

public:
  FujiDecompressor(const RawImage& img, ByteStream input);

  void decompress() const;

  struct FujiHeader {
    FujiHeader() = default;

    explicit FujiHeader(ByteStream& input_);
    explicit __attribute__((pure)) operator bool() const; // validity check

    uint16_t signature;
    uint8_t version;
    uint8_t raw_type;
    uint8_t raw_bits;
    uint16_t raw_height;
    uint16_t raw_rounded_width;
    uint16_t raw_width;
    uint16_t block_size;
    uint8_t blocks_in_row;
    uint16_t total_lines;
    iPoint2D MCU;
  };

private:
  FujiHeader header;

  void fuji_compressed_load_raw();

  struct fuji_compressed_params {
    fuji_compressed_params() = default;

    explicit fuji_compressed_params(const FujiDecompressor& d);

    std::vector<int8_t> q_table; /* quantization table */
    std::array<int, 5> q_point;  /* quantization points */
    int max_bits;
    int min_value;
    int raw_bits;
    int total_values;
    int maxDiff;
    uint16_t line_width;
  };

  fuji_compressed_params common_info;

  ByteStream input;

  std::vector<ByteStream> strips;
};

} // namespace rawspeed
