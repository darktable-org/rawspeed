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
#include "io/BitPumpLSB.h"                // for BitPumpLSB
#include <algorithm>                      // for min
#include <algorithm>                      // for generate_n, fill_n
#include <array>                          // for array
#include <cassert>                        // for assert
#include <cstring>                        // for memcpy
#include <iterator>                       // for back_inserter
#include <utility>                        // for move
#include <vector>                         // for vector

namespace rawspeed {

struct PanasonicDecompressorV5::CompressionDsc {
  int bps;
  int pixelsPerPacket;

  constexpr CompressionDsc();
  constexpr CompressionDsc(int bps_, int pixelsPerPacket_)
      : bps(bps_), pixelsPerPacket(pixelsPerPacket_) {}
};

static constexpr auto TwelveBit =
    PanasonicDecompressorV5::CompressionDsc(/*bps=*/12, /*pixelsPerPacket=*/10);
static constexpr auto FourteenBit =
    PanasonicDecompressorV5::CompressionDsc(/*bps=*/14, /*pixelsPerPacket=*/9);

PanasonicDecompressorV5::PanasonicDecompressorV5(const RawImage& img,
                                                 const ByteStream& input_,
                                                 uint32 bps_)
    : AbstractParallelizedDecompressor(img), bps(bps_) {
  if (mRaw->getCpp() != 1 || mRaw->getDataType() != TYPE_USHORT16 ||
      mRaw->getBpp() != 2)
    ThrowRDE("Unexpected component count / data type");

  const CompressionDsc* dsc = nullptr;
  switch (bps) {
  case 12:
    dsc = &TwelveBit;
    break;
  case 14:
    dsc = &FourteenBit;
    break;
  default:
    ThrowRDE("Unsupported bps: %u", bps);
  }

  if (!mRaw->dim.hasPositiveArea() || mRaw->dim.x % dsc->pixelsPerPacket != 0) {
    ThrowRDE("Unexpected image dimensions found: (%i; %i)", mRaw->dim.x,
             mRaw->dim.y);
  }

  // How many pixel packets does the specified pixel count require?
  assert(mRaw->dim.area() % dsc->pixelsPerPacket == 0);
  const auto numPackets = mRaw->dim.area() / dsc->pixelsPerPacket;
  assert(numPackets > 0);

  // And how many blocks that would be? Last block may not be full, pad it.
  numBlocks = roundUpDivision(numPackets, PacketsPerBlock);
  assert(numBlocks > 0);

  // How many full blocks does the input contain? This is truncating division.
  const auto haveBlocks = input_.getRemainSize() / BlockSize;

  // Does the input contain enough blocks?
  if (haveBlocks < numBlocks)
    ThrowRDE("Unsufficient count of input blocks for a given image");

  // We only want those blocks we need, no extras.
  input = input_.peekStream(numBlocks, BlockSize);

  chopInputIntoBlocks(*dsc);
}

void PanasonicDecompressorV5::chopInputIntoBlocks(const CompressionDsc& dsc) {
  auto pixelToCoordinate = [width = mRaw->dim.x](unsigned pixel) -> iPoint2D {
    return iPoint2D(pixel % width, pixel / width);
  };

  assert(numBlocks * BlockSize == input.getRemainSize());
  blocks.reserve(numBlocks);

  const auto pixelsPerBlock = dsc.pixelsPerPacket * PacketsPerBlock;
  assert((numBlocks - 1U) * pixelsPerBlock < mRaw->dim.area());
  assert(numBlocks * pixelsPerBlock >= mRaw->dim.area());

  unsigned currPixel = 0;
  std::generate_n(std::back_inserter(blocks), numBlocks,
                  [input = &input, &currPixel, pixelToCoordinate,
                   pixelsPerBlock]() -> Block {
                    ByteStream bs = input->getStream(BlockSize);
                    iPoint2D beginCoord = pixelToCoordinate(currPixel);
                    currPixel += pixelsPerBlock;
                    iPoint2D endCoord = pixelToCoordinate(currPixel);
                    return {std::move(bs), beginCoord, endCoord};
                  });
  assert(blocks.size() == numBlocks);
  assert(currPixel >= mRaw->dim.area());
  assert(input.getRemainSize() == 0);

  // Clamp the end coordinate for the last block.
  blocks.back().endCoord = mRaw->dim;
  blocks.back().endCoord.y -= 1;
}

class PanasonicDecompressorV5::ProxyStream {
  ByteStream block;
  std::vector<uchar8> buf;
  ByteStream input;

  void parseBlock() {
    assert(buf.empty());
    assert(block.getRemainSize() == BlockSize);

    static_assert(BlockSize > sectionSplitOffset, "");

    Buffer FirstSection = block.getBuffer(sectionSplitOffset);
    Buffer SecondSection = block.getBuffer(block.getRemainSize());
    assert(FirstSection.getSize() < SecondSection.getSize());

    buf.reserve(BlockSize);

    // First copy the second section. This makes it the first section.
    buf.insert(buf.end(), SecondSection.begin(), SecondSection.end());
    // Now append the original 1'st section right after the new 1'st section.
    buf.insert(buf.end(), FirstSection.begin(), FirstSection.end());

    assert(buf.size() == BlockSize);
    assert(block.getRemainSize() == 0);

    // And reset the clock.
    input = ByteStream(DataBuffer(Buffer(buf.data(), buf.size())));
  }

public:
  explicit ProxyStream(ByteStream block_) : block(std::move(block_)) {}

  const uchar8* getData() {
    parseBlock();
    return input.getData(BlockSize);
  }
};

template <>
void PanasonicDecompressorV5::processPixelPacket<TwelveBit>(
    ushort16* dest, const uchar8* bytes) const {
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
}

template <>
void PanasonicDecompressorV5::processPixelPacket<FourteenBit>(
    ushort16* dest, const uchar8* bytes) const {
  *dest++ = bytes[0] + ((bytes[1] & 0x3F) << 8);
  *dest++ = (bytes[1] >> 6) + 4 * (bytes[2]) + ((bytes[3] & 0xF) << 10);
  *dest++ = (bytes[3] >> 4) + 16 * (bytes[4]) + ((bytes[5] & 3) << 12);
  *dest++ = ((bytes[5] & 0xFC) >> 2) + (bytes[6] << 6);

  *dest++ = bytes[7] + ((bytes[8] & 0x3F) << 8);
  *dest++ = (bytes[8] >> 6) + 4 * bytes[9] + ((bytes[10] & 0xF) << 10);
  *dest++ = (bytes[10] >> 4) + 16 * bytes[11] + ((bytes[12] & 3) << 12);
  *dest++ = ((bytes[12] & 0xFC) >> 2) + (bytes[13] << 6);

  *dest++ = bytes[14] + ((bytes[15] & 0x3F) << 8);
}

template <const PanasonicDecompressorV5::CompressionDsc& dsc>
void PanasonicDecompressorV5::processBlock(const Block& block) const {
  static_assert(BlockSize % bytesPerPacket == 0, "");

  ProxyStream proxy(block.bs);
  const uchar8* bytes = proxy.getData();

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

    assert(x % dsc.pixelsPerPacket == 0);
    assert(endx % dsc.pixelsPerPacket == 0);

    for (; x < endx;) {
      processPixelPacket<dsc>(dest, bytes);

      x += dsc.pixelsPerPacket;
      dest += dsc.pixelsPerPacket;
      bytes += bytesPerPacket;
    }
  }
}

template <const PanasonicDecompressorV5::CompressionDsc& dsc>
void PanasonicDecompressorV5::decompressThreadedInternal(
    const RawDecompressorThread* t) const {
  assert(t->start < t->end);
  assert(t->end <= blocks.size());
  for (size_t i = t->start; i < t->end; i++)
    processBlock<dsc>(blocks[i]);
}

void PanasonicDecompressorV5::decompressThreaded(
    const RawDecompressorThread* t) const {
  switch (bps) {
  case 12:
    decompressThreadedInternal<TwelveBit>(t);
    break;
  case 14:
    decompressThreadedInternal<FourteenBit>(t);
    break;
  default:
    __builtin_unreachable();
  }
}

void PanasonicDecompressorV5::decompress() const {
  assert(blocks.size() == numBlocks);
  startThreading(blocks.size());
}

} // namespace rawspeed
