/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2014 Pedro Côrte-Real

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

#include "common/Common.h"                      // for uint32, uchar8
#include "decompressors/AbstractDecompressor.h" // for AbstractDecompressor
#include "io/BitPumpJPEG.h"                     // for BitPumpJPEG
#include <array>                                // for array

namespace RawSpeed {

class Buffer;

class RawImage;

class HuffmanTable;

class CrwDecompressor final : public AbstractDecompressor {
public:
  static void decompress(RawImage& mRaw, RawSpeed::Buffer* mFile,
                         uint32 dec_table, bool lowbits);

private:
  static HuffmanTable makeDecoder(int n, const uchar8* source);
  static std::array<HuffmanTable, 2> initHuffTables(uint32 table);

  inline static void decodeBlock(std::array<int, 64>* diffBuf,
                                 const std::array<HuffmanTable, 2>& mHuff,
                                 BitPumpJPEG* pump);
};

} // namespace RawSpeed
