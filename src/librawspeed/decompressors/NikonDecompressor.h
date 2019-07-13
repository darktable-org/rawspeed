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

#include "common/Common.h"                      // for uint32, uint16_t
#include "common/RawImage.h"                    // for RawImage
#include "decompressors/AbstractDecompressor.h" // for AbstractDecompressor
#include "io/BitPumpMSB.h"                      // for BitPumpMSB
#include <vector>                               // for vector

namespace rawspeed {

class ByteStream;

class NikonDecompressor final : public AbstractDecompressor {
  RawImage mRaw;
  uint32 bitsPS;

  uint32 huffSelect = 0;
  uint32 split = 0;

  std::array<int, 2> pUp1;
  std::array<int, 2> pUp2;

  std::vector<uint16_t> curve;

  uint32 random;

public:
  NikonDecompressor(const RawImage& raw, ByteStream metadata, uint32 bitsPS);

  void decompress(const ByteStream& data, bool uncorrectedRawValues);

private:
  static const std::array<std::array<std::array<uint8_t, 16>, 2>, 6> nikon_tree;
  static std::vector<uint16_t> createCurve(ByteStream* metadata, uint32 bitsPS,
                                           uint32 v0, uint32 v1, uint32* split);

  template <typename Huffman>
  void decompress(BitPumpMSB* bits, int start_y, int end_y);

  template <typename Huffman>
  static Huffman createHuffmanTable(uint32 huffSelect);
};

} // namespace rawspeed
