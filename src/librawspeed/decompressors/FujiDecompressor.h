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

#include "rawspeedconfig.h"
#include "adt/Array1DRef.h"
#include "adt/Point.h"
#include "common/RawImage.h"
#include "decompressors/AbstractDecompressor.h"
#include "io/ByteStream.h"
#include <cstdint>
#include <vector>

namespace rawspeed {

class FujiDecompressor final : public AbstractDecompressor {
  RawImage mRaw;

public:
  FujiDecompressor(RawImage img, ByteStream input);

  void decompress() const;

  struct FujiHeader final {
    FujiHeader() = default;

    explicit FujiHeader(ByteStream& input_);
    explicit RAWSPEED_READONLY operator bool() const; // validity check

    // 1 = lossless compressed RAF, 0 = lossy compressed RAF.
    // (rawler/dnglab calls the equivalent header field "lossless" instead
    // of "version". Confirmed correct: real lossy-compressed RAF samples
    // parse and decode pixel-exact against dnglab/rawler's decode.)
    [[nodiscard]] bool isLossless() const { return version == 1; }

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

  ByteStream input;

  std::vector<Array1DRef<const uint8_t>> strips;

  // Only populated for lossy-compressed RAFs (header.isLossless() == false).
  // One byte per line, per strip; each strip's run is padded up to a
  // multiple of 16 bytes. See FujiDecompressor::FujiDecompressor() for the
  // exact layout, and fuji_compressed_block::fuji_decode_strip() (in the
  // .cpp) for how it's consumed.
  std::vector<uint8_t> q_bases;

  // Stride between strips within q_bases (each strip's run of per-line
  // q_bases, padded up to a multiple of 16 bytes). 0 for lossless files
  // (q_bases is empty and unused in that case).
  int q_bases_line_step = 0;
};

} // namespace rawspeed
