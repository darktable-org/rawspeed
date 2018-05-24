/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2018 Alexey Danilchenko
    Copyright (C) 2018 Stefan Hoffmeister
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

#include "decompressors/PanasonicDecompressorV5.h"
#include "common/Mutex.h"                 // for MutexLocker
#include "common/Point.h"                 // for iPoint2D
#include "common/RawImage.h"              // for RawImage, RawImageData
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include <algorithm>                      // for min
#include <algorithm>                      // for generate_n, fill_n
#include <array>                          // for array
#include <cassert>                        // for assert
#include <cstring>                        // for memcpy
#include <iterator>                       // for back_inserter
#include <memory>                         // for uninitialized_copy
#include <utility>                        // for move
#include <vector>                         // for vector

namespace rawspeed {

PanasonicDecompressorV5::PanasonicDecompressorV5(const RawImage& img,
                                                 const ByteStream& input_,
                                                 uint32 bps_)
    : mRaw(img), bps(bps_) {
  if (mRaw->getCpp() != 1 || mRaw->getDataType() != TYPE_USHORT16 ||
      mRaw->getBpp() != 2)
    ThrowRDE("Unexpected component count / data type");

  switch (bps) {
  case 12:
    pixelsPerPacket = 10;
    break;
  case 14:
    pixelsPerPacket = 9;
    break;
  default:
    ThrowRDE("Unsupported bps: %u", bps);
  }

  if (mRaw->dim.x % pixelsPerPacket != 0)
    ThrowRDE("Image width is not a multiple of pixels-per-packet");

  assert(mRaw->dim.area() % pixelsPerPacket == 0);
  const auto numPackets = mRaw->dim.area() / pixelsPerPacket;
  const auto packetsTotalSize = numPackets * bytesPerPacket;
  const auto numBlocks = roundUpDivision(packetsTotalSize, BlockSize);
  const auto blocksTotalSize = numBlocks * BlockSize;
  input = input_.peekStream(blocksTotalSize);

  chopInputIntoWorkBlocks();
}

void PanasonicDecompressorV5::chopInputIntoWorkBlocks() {
  auto pixelToCoordinate = [width = mRaw->dim.x](unsigned pixel) -> iPoint2D {
    return iPoint2D(pixel % width, pixel / width);
  };

  static_assert(BlockSize % bytesPerPacket == 0, "");
  const auto pixelsPerBlock = pixelsPerPacket * (BlockSize / bytesPerPacket);

  assert(input.getRemainSize() % BlockSize == 0);
  const auto numBlocks = input.getRemainSize() / BlockSize;
  blocks.reserve(numBlocks);

  assert((numBlocks - 1U) * pixelsPerBlock < mRaw->dim.area());
  assert(numBlocks * pixelsPerBlock >= mRaw->dim.area());

  unsigned currPixel = 0;
  std::generate_n(std::back_inserter(blocks), numBlocks,
                  [input = &input, &currPixel, pixelToCoordinate,
                   pixelsPerBlock]() -> Block {
                    ByteStream inputBs = input->getStream(BlockSize);
                    iPoint2D beginCoord = pixelToCoordinate(currPixel);
                    currPixel += pixelsPerBlock;
                    iPoint2D endCoord = pixelToCoordinate(currPixel);
                    return {std::move(inputBs), beginCoord, endCoord};
                  });

  // Clamp the end coordinate for the last block.
  blocks[numBlocks - 1U].endCoord = mRaw->dim;
  blocks[numBlocks - 1U].endCoord.y -= 1;
}

class PanasonicDecompressorV5::DataPump {
  ByteStream blocks;
  std::vector<uchar8> buf;
  ByteStream input;

  void parseBlock() {
    if (input.getRemainSize() > 0)
      return;

    assert(buf.size() == BlockSize);
    static_assert(BlockSize > section_split_offset, "");

    ByteStream thisBlock = blocks.getStream(BlockSize);

    Buffer FirstSection = thisBlock.getBuffer(section_split_offset);
    Buffer SecondSection = thisBlock.getBuffer(thisBlock.getRemainSize());
    assert(FirstSection.getSize() < SecondSection.getSize());

    // First copy the second section. This makes it the first section.
    auto bufDst = buf.begin();
    bufDst = std::uninitialized_copy(SecondSection.begin(), SecondSection.end(),
                                     bufDst);
    // Now append the original 1'st section right after the new 1'st section.
    bufDst = std::uninitialized_copy(FirstSection.begin(), FirstSection.end(),
                                     bufDst);
    assert(buf.end() == bufDst);

    assert(thisBlock.getRemainSize() == 0);

    // And reset the clock.
    input = ByteStream(DataBuffer(Buffer(buf.data(), buf.size())));
  }

public:
  explicit DataPump(ByteStream blocks_) : blocks(std::move(blocks_)) {
    static_assert(BlockSize % bytesPerPacket == 0, "");
    buf.resize(BlockSize);
  }

  const uchar8* readBlock() {
    parseBlock();
    return input.getData(BlockSize);
  }
};

void PanasonicDecompressorV5::processPixelGroup(ushort16* dest,
                                                const uchar8* bytes) const {
  if (bps == 12) {
    *dest++ = ((bytes[1] & 0xF) << 8) + bytes[0];
    *dest++ = 16 * bytes[2] + (bytes[1] >> 4);

    *dest++ = ((bytes[4] & 0xF) << 8) + bytes[3];
    *dest++ = 16 * bytes[5] + (bytes[4] >> 4);

    *dest++ = ((bytes[7] & 0xF) << 8) + bytes[6];
    *dest++ = 16 * bytes[8] + (bytes[7] >> 4);

    *dest++ = ((bytes[10] & 0xF) << 8) + bytes[9];
    *dest++ = 16 * bytes[11] + (bytes[10] >> 4);

    *dest++ = ((bytes[13] & 0xF) << 8) + bytes[12];
    *dest++ = 16 * bytes[14] + (bytes[13] >> 4);

    // FIXME badPixelTracker needs to be filled in case of bad pixels
  } else if (bps == 14) {
    *dest++ = bytes[0] + ((bytes[1] & 0x3F) << 8);
    *dest++ = (bytes[1] >> 6) + 4 * (bytes[2]) + ((bytes[3] & 0xF) << 10);
    *dest++ = (bytes[3] >> 4) + 16 * (bytes[4]) + ((bytes[5] & 3) << 12);
    *dest++ = ((bytes[5] & 0xFC) >> 2) + (bytes[6] << 6);

    *dest++ = bytes[7] + ((bytes[8] & 0x3F) << 8);
    *dest++ = (bytes[8] >> 6) + 4 * bytes[9] + ((bytes[10] & 0xF) << 10);
    *dest++ = (bytes[10] >> 4) + 16 * bytes[11] + ((bytes[12] & 3) << 12);
    *dest++ = ((bytes[12] & 0xFC) >> 2) + (bytes[13] << 6);

    *dest++ = bytes[14] + ((bytes[15] & 0x3F) << 8);

    // FIXME badPixelTracker needs to be filled in case of bad pixels
  }
}

void PanasonicDecompressorV5::processBlock(const Block& block) const {
  DataPump pump(block.inputBs);
  const uchar8* bytes = pump.readBlock();

  for (int y = block.beginCoord.y; y <= block.endCoord.y; y++) {
    int x = 0;
    // First row may not begin at the first column.
    if (block.beginCoord.y == y)
      x = block.beginCoord.x;

    int endx = mRaw->dim.x;
    // Last row may end before the last column.
    if (block.endCoord.y == y)
      endx = block.endCoord.x;

    auto* dest = reinterpret_cast<ushort16*>(mRaw->getData(x, y));

    assert(x % pixelsPerPacket == 0);
    assert(endx % pixelsPerPacket == 0);

    for (; x < endx;) {
      processPixelGroup(dest, bytes);

      x += pixelsPerPacket;
      dest += pixelsPerPacket;
      bytes += bytesPerPacket;
    }
  }
}

void PanasonicDecompressorV5::decompress() const {
  assert(!blocks.empty());
  for (const Block& block : blocks)
    processBlock(block);
}

} // namespace rawspeed
