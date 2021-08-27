/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2014 Pedro Côrte-Real
    Copyright (C) 2017-2018 Roman Lebedev

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

#include "rawspeedconfig.h" // for HAVE_OPENMP
#include "decompressors/PanasonicDecompressorV4.h"
#include "common/Array2DRef.h"            // for Array2DRef
#include "common/Common.h"                // for extractHighBits, rawspeed_...
#include "common/Mutex.h"                 // for MutexLocker
#include "common/Point.h"                 // for iPoint2D
#include "common/RawImage.h"              // for RawImage, RawImageData
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "io/Buffer.h"                    // for Buffer, Buffer::size_type
#include <algorithm>                      // for max, generate_n, min
#include <array>                          // for array
#include <cassert>                        // for assert
#include <cstdint>                        // for uint32_t, uint8_t, uint16_t
#include <iterator>                       // for back_insert_iterator, back...
#include <limits>                         // for numeric_limits
#include <memory>                         // for allocator_traits<>::value_...
#include <utility>                        // for move
#include <vector>                         // for vector

namespace rawspeed {

PanasonicDecompressorV4::PanasonicDecompressorV4(const RawImage& img,
                                                 const ByteStream& input_,
                                                 bool zero_is_not_bad,
                                                 uint32_t section_split_offset_)
    : mRaw(img), zero_is_bad(!zero_is_not_bad),
      section_split_offset(section_split_offset_) {
  if (mRaw->getCpp() != 1 || mRaw->getDataType() != TYPE_USHORT16 ||
      mRaw->getBpp() != sizeof(uint16_t))
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

  if (bufSize > std::numeric_limits<ByteStream::size_type>::max())
    ThrowRDE("Raw dimensions require input buffer larger than supported");

  input = input_.peekStream(bufSize);

  chopInputIntoBlocks();
}

void PanasonicDecompressorV4::chopInputIntoBlocks() {
  auto pixelToCoordinate = [width = mRaw->dim.x](unsigned pixel) {
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

class PanasonicDecompressorV4::ProxyStream {
  ByteStream block;
  const uint32_t section_split_offset;
  std::vector<uint8_t> buf;

  int vbits = 0;

  void parseBlock() {
    assert(buf.empty());
    assert(block.getRemainSize() <= BlockSize);
    assert(section_split_offset <= BlockSize);

    Buffer FirstSection = block.getBuffer(section_split_offset);
    Buffer SecondSection = block.getBuffer(block.getRemainSize());

    // get one more byte, so the return statement of getBits does not have
    // to special case for accessing the last byte
    buf.reserve(BlockSize + 1UL);

    // First copy the second section. This makes it the first section.
    buf.insert(buf.end(), SecondSection.begin(), SecondSection.end());
    // Now append the original 1'st section right after the new 1'st section.
    buf.insert(buf.end(), FirstSection.begin(), FirstSection.end());

    assert(block.getRemainSize() == 0);

    // get one more byte, so the return statement of getBits does not have
    // to special case for accessing the last byte
    buf.emplace_back(0);
  }

public:
  ProxyStream(ByteStream block_, int section_split_offset_)
      : block(std::move(block_)), section_split_offset(section_split_offset_) {
    parseBlock();
  }

  uint32_t getBits(int nbits) noexcept {
    vbits = (vbits - nbits) & 0x1ffff;
    int byte = vbits >> 3 ^ 0x3ff0;
    return (buf[byte] | buf[byte + 1UL] << 8) >> (vbits & 7) & ~(-(1 << nbits));
  }
};

inline void PanasonicDecompressorV4::processPixelPacket(
    ProxyStream& bits, int row, int col,
    std::vector<uint32_t>* zero_pos) const noexcept {
  const Array2DRef<uint16_t> out(mRaw->getU16DataAsUncroppedArray2DRef());

  int sh = 0;

  std::array<int, 2> pred;
  pred.fill(0);

  std::array<int, 2> nonz;
  nonz.fill(0);

  int u = 0;

  for (int p = 0; p < PixelsPerPacket; ++p, ++col) {
    const int c = p & 1;

    // FIXME: this is actually just `p % 3 == 2`, cleanup after perf is good.
    if (u == 2) {
      sh = extractHighBits(4U, bits.getBits(2), /*effectiveBitwidth=*/3);
      u = -1;
    }

    if (nonz[c]) {
      int j = bits.getBits(8);
      if (j) {
        pred[c] -= 0x80 << sh;
        if (pred[c] < 0 || sh == 4)
          pred[c] &= ~(-(1 << sh));
        pred[c] += j << sh;
      }
    } else {
      nonz[c] = bits.getBits(8);
      if (nonz[c] || p > 11)
        pred[c] = nonz[c] << 4 | bits.getBits(4);
    }

    out(row, col) = pred[c];

    if (zero_is_bad && 0 == pred[c])
      zero_pos->push_back((row << 16) | col);

    u++;
  }
}

void PanasonicDecompressorV4::processBlock(
    const Block& block, std::vector<uint32_t>* zero_pos) const noexcept {
  ProxyStream bits(block.bs, section_split_offset);

  for (int row = block.beginCoord.y; row <= block.endCoord.y; row++) {
    int col = 0;
    // First row may not begin at the first column.
    if (block.beginCoord.y == row)
      col = block.beginCoord.x;

    int endCol = mRaw->dim.x;
    // Last row may end before the last column.
    if (block.endCoord.y == row)
      endCol = block.endCoord.x;

    assert(col % PixelsPerPacket == 0);
    assert(endCol % PixelsPerPacket == 0);

    for (; col < endCol; col += PixelsPerPacket)
      processPixelPacket(bits, row, col, zero_pos);
  }
}

void PanasonicDecompressorV4::decompressThread() const noexcept {
  std::vector<uint32_t> zero_pos;

  assert(!blocks.empty());

#ifdef HAVE_OPENMP
#pragma omp for schedule(static)
#endif
  for (auto block = blocks.cbegin(); block < blocks.cend(); ++block)
    processBlock(*block, &zero_pos);

  if (zero_is_bad && !zero_pos.empty()) {
    MutexLocker guard(&mRaw->mBadPixelMutex);
    mRaw->mBadPixelPositions.insert(mRaw->mBadPixelPositions.end(),
                                    zero_pos.begin(), zero_pos.end());
  }
}

void PanasonicDecompressorV4::decompress() const noexcept {
  assert(!blocks.empty());
#ifdef HAVE_OPENMP
#pragma omp parallel default(none)                                             \
    num_threads(rawspeed_get_number_of_processor_cores())
#endif
  decompressThread();
}

} // namespace rawspeed
