/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post

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

#include "adt/Array1DRef.h"
#include "bitstreams/BitStreamerMSB.h"
#include "codes/PrefixCodeDecoder.h"
#include "common/RawImage.h"
#include "decompressors/AbstractDecompressor.h"
#include <array>
#include <cstdint>
#include <vector>

namespace rawspeed {
class ByteStream;
} // namespace rawspeed

namespace rawspeed {

class NikonDecompressor final : public AbstractDecompressor {
  RawImage mRaw;
  uint32_t bitsPS;

  uint32_t huffSelect = 0;
  uint32_t split = 0;

  std::array<std::array<int, 2>, 2> pUp;

  std::vector<uint16_t> curve;

  uint32_t random;

public:
  NikonDecompressor(RawImage raw, ByteStream metadata, uint32_t bitsPS);

  void decompress(Array1DRef<const uint8_t> input, bool uncorrectedRawValues);

private:
  static const std::array<std::array<std::array<uint8_t, 16>, 2>, 6> nikon_tree;
  static std::vector<uint16_t> createCurve(ByteStream& metadata,
                                           uint32_t bitsPS, uint32_t v0,
                                           uint32_t v1, uint32_t* split);

  template <typename Huffman>
  void decompress(BitStreamerMSB& bits, int start_y, int end_y);

  template <typename Huffman>
  static Huffman createPrefixCodeDecoder(uint32_t huffSelect);
};

template <>
PrefixCodeDecoder<>
NikonDecompressor::createPrefixCodeDecoder<PrefixCodeDecoder<>>(
    uint32_t huffSelect);

} // namespace rawspeed
