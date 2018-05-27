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

#include "common/Common.h"   // for uint32
#include "common/Point.h"    // for iPoint2D
#include "common/RawImage.h" // for RawImage
#include "decompressors/AbstractParallelizedDecompressor.h"
#include "io/ByteStream.h" // for ByteStream
#include <cstddef>         // for size_t
#include <utility>         // for move
#include <vector>          // for vector

namespace rawspeed {

class RawImage;

class PanasonicDecompressor final : public AbstractParallelizedDecompressor {
  static constexpr uint32 BlockSize = 0x4000;

  static constexpr int PixelsPerPacket = 14;

  static constexpr uint32 BytesPerPacket = 16;

  static constexpr uint32 PacketsPerBlock = BlockSize / BytesPerPacket;

  static constexpr uint32 PixelsPerBlock = PixelsPerPacket * PacketsPerBlock;

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

  struct Block {
    ByteStream bs;
    iPoint2D beginCoord;
    // The rectangle is an incorrect representation. All the rows
    // between the first and last one span the entire width of the image.
    iPoint2D endCoord;

    Block() = default;
    Block(ByteStream&& bs_, iPoint2D beginCoord_, iPoint2D endCoord_)
        : bs(std::move(bs_)), beginCoord(beginCoord_), endCoord(endCoord_) {}
  };

  // If really wanted, this vector could be avoided,
  // and each Block computed on-the-fly
  std::vector<Block> blocks;

  void chopInputIntoBlocks();

  void processPixelPacket(PanaBitpump* bits, int y, ushort16* dest, int xbegin,
                          std::vector<uint32>* zero_pos) const;

  void processBlock(const Block& block, std::vector<uint32>* zero_pos) const;

  void decompressThreaded(const RawDecompressorThread* t) const final;

public:
  PanasonicDecompressor(const RawImage& img, const ByteStream& input_,
                        bool zero_is_not_bad, uint32 section_split_offset_);

  void decompress() const final;
};

} // namespace rawspeed
