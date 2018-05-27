/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2014 Pedro Côrte-Real
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

#include "decompressors/PanasonicDecompressor.h"
#include "common/Mutex.h"                 // for MutexLocker
#include "common/Point.h"                 // for iPoint2D
#include "common/RawImage.h"              // for RawImage, RawImageData
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include <algorithm>                      // for min
#include <array>                          // for array
#include <cassert>                        // for assert
#include <cstring>                        // for memcpy
#include <utility>                        // for move
#include <vector>                         // for vector

namespace rawspeed {

constexpr uint32 PanasonicDecompressor::BlockSize;

PanasonicDecompressor::PanasonicDecompressor(const RawImage& img,
                                             const ByteStream& input_,
                                             bool zero_is_not_bad,
                                             uint32 section_split_offset_)
    : AbstractParallelizedDecompressor(img), zero_is_bad(!zero_is_not_bad),
      section_split_offset(section_split_offset_) {
  if (mRaw->getCpp() != 1 || mRaw->getDataType() != TYPE_USHORT16 ||
      mRaw->getBpp() != 2)
    ThrowRDE("Unexpected component count / data type");

  if (!mRaw->dim.hasPositiveArea() || mRaw->dim.x % PixelsPerPacket != 0) {
    ThrowRDE("Unexpected image dimensions found: (%i; %i)", mRaw->dim.x,
             mRaw->dim.y);
  }

  if (BlockSize < section_split_offset)
    ThrowRDE("Bad section_split_offset: %u, less than BlockSize (%u)",
             section_split_offset, BlockSize);

  // Naive count of bytes that given pixel count requires.
  assert(mRaw->dim.area() % PixelsPerPacket == 0);
  const auto bytesTotal = (mRaw->dim.area() / PixelsPerPacket) * BytesPerPacket;
  assert(bytesTotal > 0);

  // If section_split_offset is zero, then that we need to read the normal
  // amount of bytes. But if it is not, then we need to round up to multiple of
  // BlockSize, because of splitting&rotation of each BlockSize's slice in half
  // at section_split_offset bytes.
  const auto bufSize =
      section_split_offset == 0 ? bytesTotal : roundUp(bytesTotal, BlockSize);
  input = input_.peekStream(bufSize);

  chopInputIntoBlocks();
}

void PanasonicDecompressor::chopInputIntoBlocks() {
  auto pixelToCoordinate = [width = mRaw->dim.x](unsigned pixel) -> iPoint2D {
    return iPoint2D(pixel % width, pixel / width);
  };

  // If section_split_offset == 0, last block may not be full.
  const auto blocksTotal = roundUpDivision(input.getRemainSize(), BlockSize);
  assert(blocksTotal > 0);
  assert(blocksTotal * PixelsPerBlock >= mRaw->dim.area());
  blocks.reserve(blocksTotal);

  unsigned currPixel = 0;
  std::generate_n(std::back_inserter(blocks), blocksTotal,
                  [input = &input, &currPixel, pixelToCoordinate]() -> Block {
                    assert(input->getRemainSize() != 0);
                    const auto blockSize =
                        std::min(input->getRemainSize(), BlockSize);
                    assert(blockSize > 0);
                    assert(blockSize % BytesPerPacket == 0);
                    const auto packets = blockSize / BytesPerPacket;
                    assert(packets > 0);
                    const auto pixels = packets * PixelsPerPacket;
                    assert(pixels > 0);

                    ByteStream bs = input->getStream(blockSize);
                    iPoint2D beginCoord = pixelToCoordinate(currPixel);
                    currPixel += pixels;
                    iPoint2D endCoord = pixelToCoordinate(currPixel);
                    return {std::move(bs), beginCoord, endCoord};
                  });
  assert(blocks.size() == blocksTotal);
  assert(currPixel >= mRaw->dim.area());
  assert(input.getRemainSize() == 0);

  // Clamp the end coordinate for the last block.
  blocks.back().endCoord = mRaw->dim;
  blocks.back().endCoord.y -= 1;
}

struct PanasonicDecompressor::PanaBitpump {
  ByteStream input;
  std::vector<uchar8> buf;
  int vbits = 0;
  uint32 section_split_offset;

  PanaBitpump(ByteStream input_, int section_split_offset_)
      : input(std::move(input_)), section_split_offset(section_split_offset_) {
    // get one more byte, so the return statement of getBits does not have
    // to special case for accessing the last byte
    buf.resize(BlockSize + 1UL);
  }

  uint32 getBits(int nbits) {
    if (!vbits) {
      /* On truncated files this routine will just return for the truncated
       * part of the file. Since there is no chance of affecting output buffer
       * size we allow the decoder to decode this
       */
      assert(BlockSize >= section_split_offset);
      auto size =
          std::min(input.getRemainSize(), BlockSize - section_split_offset);
      memcpy(buf.data() + section_split_offset, input.getData(size), size);

      size = std::min(input.getRemainSize(), section_split_offset);
      if (size != 0)
        memcpy(buf.data(), input.getData(size), size);
    }
    vbits = (vbits - nbits) & 0x1ffff;
    int byte = vbits >> 3 ^ 0x3ff0;
    return (buf[byte] | buf[byte + 1UL] << 8) >> (vbits & 7) & ~(-(1 << nbits));
  }
};

void PanasonicDecompressor::processPixelPacket(
    PanaBitpump* bits, int y, ushort16* dest, int xbegin,
    std::vector<uint32>* zero_pos) const {
  int sh = 0;

  std::array<int, 2> pred;
  pred.fill(0);

  std::array<int, 2> nonz;
  nonz.fill(0);

  int u = 0;

  for (int p = 0; p < PixelsPerPacket; p++) {
    const int c = p & 1;

    if (u == 2) {
      sh = 4 >> (3 - bits->getBits(2));
      u = -1;
    }

    if (nonz[c]) {
      int j = bits->getBits(8);
      if (j) {
        pred[c] -= 0x80 << sh;
        if (pred[c] < 0 || sh == 4)
          pred[c] &= ~(-(1 << sh));
        pred[c] += j << sh;
      }
    } else {
      nonz[c] = bits->getBits(8);
      if (nonz[c] || p > 11)
        pred[c] = nonz[c] << 4 | bits->getBits(4);
    }

    *dest = pred[c];

    if (zero_is_bad && 0 == pred[c])
      zero_pos->push_back((y << 16) | (xbegin + p));

    u++;
    dest++;
  }
}

void PanasonicDecompressor::processBlock(const Block& block,
                                         std::vector<uint32>* zero_pos) const {
  PanaBitpump bits(block.bs, section_split_offset);

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

    assert(x % PixelsPerPacket == 0);
    assert(endx % PixelsPerPacket == 0);

    for (; x < endx;) {
      processPixelPacket(&bits, y, dest, x, zero_pos);

      x += PixelsPerPacket;
      dest += PixelsPerPacket;
    }
  }
}

void PanasonicDecompressor::decompressThreaded(
    const RawDecompressorThread* t) const {
  std::vector<uint32> zero_pos;

  assert(!blocks.empty());
  assert(t->start < t->end);
  assert(t->end <= blocks.size());
  for (size_t i = t->start; i < t->end; i++)
    processBlock(blocks[i], &zero_pos);

  if (zero_is_bad && !zero_pos.empty()) {
    MutexLocker guard(&mRaw->mBadPixelMutex);
    mRaw->mBadPixelPositions.insert(mRaw->mBadPixelPositions.end(),
                                    zero_pos.begin(), zero_pos.end());
  }
}

void PanasonicDecompressor::decompress() const {
  assert(!blocks.empty());
  startThreading(blocks.size());
}

} // namespace rawspeed
