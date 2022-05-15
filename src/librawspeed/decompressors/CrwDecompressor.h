/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2014 Pedro Côrte-Real
    Copyright (C) 2018 Roman Lebedev

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
#include "decompressors/AbstractDecompressor.h" // for AbstractDecompressor
#include "decompressors/HuffmanTable.h"         // for HuffmanTable
#include "io/BitPumpJPEG.h"                     // for BitPumpJPEG
#include "io/ByteStream.h"                      // for ByteStream
#include <array>                                // for array
#include <cstdint>                              // for uint32_t, uint8_t

namespace rawspeed {

class CrwDecompressor final : public AbstractDecompressor {
  using crw_hts = std::array<HuffmanTable, 2>;

  RawImageData* mRaw;
  crw_hts mHuff;
  const bool lowbits;

  ByteStream lowbitInput;
  ByteStream rawInput;

public:
  CrwDecompressor(RawImageData *img, uint32_t dec_table_, bool lowbits_,
                  ByteStream rawData);

  void decompress();

private:
  static HuffmanTable makeDecoder(const uint8_t* ncpl, const uint8_t* values);
  static crw_hts initHuffTables(uint32_t table);

  inline static void decodeBlock(std::array<int16_t, 64>* diffBuf,
                                 const crw_hts& mHuff, BitPumpJPEG& bs);
};

} // namespace rawspeed
