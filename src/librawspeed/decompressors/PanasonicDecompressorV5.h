/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2018 Roman Lebedev
    Copyright (C) 2018 Stefan Hoffmeister

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
#include "common/Point.h"                       // for iPoint2D
#include "common/RawImage.h"                    // for RawImage
#include "decompressors/AbstractDecompressor.h" // for AbstractDecompressor
#include "io/BitPumpLSB.h"                      // for BitPumpLSB
#include "io/ByteStream.h"                      // for ByteStream
#include <cstddef>                              // for size_t
#include <utility>                              // for move
#include <vector>                               // for vector


namespace rawspeed {

class PanasonicDecompressorV5 final : public AbstractDecompressor {
  // The RW2 raw image buffer consists of individual blocks,
  // each one BlockSize bytes in size.
  static constexpr uint32 BlockSize = 0x4000;

  // These blocks themselves comprise of two sections,
  // split and swapped at section_split_offset:
  //   bytes:  [0..sectionSplitOffset-1][sectionSplitOffset..BlockSize-1]
  //   pixels: [a..b][0..a-1]
  // When reading, these two sections need to be swapped to enable linear
  // processing..
  static constexpr uint32 sectionSplitOffset = 0x1FF8;

  // The blocks themselves consist of packets with fixed size of bytesPerPacket,
  // and each packet decodes to pixelsPerPacket pixels, which depends on bps.
  static constexpr uint32 bytesPerPacket = 16;
  static constexpr uint32 bitsPerPacket = 8 * bytesPerPacket;
  static_assert(BlockSize % bytesPerPacket == 0, "");
  static constexpr uint32 PacketsPerBlock = BlockSize / bytesPerPacket;

public:
  // Contains the decoding recepie for the packet,
  struct PacketDsc {
    int bps;
    int pixelsPerPacket;

    explicit constexpr PacketDsc(int bps_)
      : bps(bps_),
      pixelsPerPacket(PanasonicDecompressorV5::bitsPerPacket / bps) {
	// NOTE: the division is truncating. There may be some padding bits left.
      }
  };

 private:
  // Takes care of unsplitting&swapping back the block at sectionSplitOffset.
  class ProxyStream;

  RawImage mRaw;

  // The full input buffer, containing all the blocks.
  ByteStream input;

  const uint32 bps;

  size_t numBlocks;

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

  void chopInputIntoBlocks(const PacketDsc& dsc);

  template <const PacketDsc& dsc>
  void processPixelPacket(BitPumpLSB* bs, ushort16* dest) const;

  template <const PacketDsc& dsc> void processBlock(const Block& block) const;

  template <const PacketDsc& dsc> void decompressInternal() const noexcept;

public:
  PanasonicDecompressorV5(const RawImage& img, const ByteStream& input_,
                          uint32 bps_);

  void decompress() const noexcept;
};

} // namespace rawspeed
