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

#include "common/Common.h"                      // for uint32
#include "common/RawImage.h"                    // for RawImage
#include "decompressors/AbstractDecompressor.h" // for AbstractDecompressor
#include "io/ByteStream.h"                      // for ByteStream

namespace rawspeed {

class RawImage;

class PanasonicDecompressor final : public AbstractDecompressor {
  RawImage mRaw;

  static constexpr uint32 BlockSize = 0x4000;

  static constexpr int PixelsPerPacket = 14;

  static constexpr uint32 BytesPerPacket = 16;

  static constexpr uint32 PacketsPerBlock = BlockSize / BytesPerPacket;

  int packetsPerRow;

  struct PanaBitpump;

  ByteStream input;
  bool zero_is_bad;

  // The RW2 raw image buffer is split into sections of BufSize bytes.
  // If section_split_offset is 0, then the last section is not neccesarily
  // full. If section_split_offset is not 0, then each section has two parts:
  //     bytes: [0..section_split_offset-1][section_split_offset..BufSize-1]
  //     pixels: [a..b][0..a-1]
  //   I.e. these two parts need to be swapped around.
  uint32 section_split_offset;

  void processPacket(PanaBitpump* bits, int y, ushort16* dest, int block,
                     std::vector<uint32>* zero_pos) const;

public:
  PanasonicDecompressor(const RawImage& img, const ByteStream& input_,
                        bool zero_is_not_bad, uint32 section_split_offset_);

  void decompress() const;
};

} // namespace rawspeed
