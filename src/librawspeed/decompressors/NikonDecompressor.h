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

#include "common/Common.h"                      // for uint32
#include "common/RawImage.h"                    // for RawImage
#include "decompressors/AbstractDecompressor.h" // for AbstractDecompressor
#include "io/BitPumpMSB.h"                      // for BitPumpMSB
#include <array>                                // for array
#include <vector>                               // for vector

namespace rawspeed {

class iPoint2D;

class RawImage;

class ByteStream;

class HuffmanTable;

class NikonDecompressor final : public AbstractDecompressor {
  RawImage mRaw;
  uint32 bitsPS;

  uint32 huffSelect;
  uint32 split;

  int pUp1[2];
  int pUp2[2];

  std::vector<ushort16> curve;

  uint32 random;

public:
  NikonDecompressor(const RawImage& raw, ByteStream metadata, uint32 bitsPS);

  void decompress(const ByteStream& data, bool uncorrectedRawValues);

private:
  static const uchar8 nikon_tree[][2][16];
  static std::vector<ushort16> createCurve(ByteStream* metadata, uint32 bitsPS,
                                           uint32 v0, uint32 v1, uint32* split);

  template <typename Huffman>
  void decompress(BitPumpMSB* bits, int start_y, int end_y);

  template <typename Huffman>
  static Huffman createHuffmanTable(uint32 huffSelect);
};

} // namespace rawspeed
