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

#include "rawspeedconfig.h" // for HAVE_OPENMP
#include "decompressors/PanasonicDecompressorV5.h"
#include "common/Array2DRef.h"            // for Array2DRef
#include "common/Common.h"                // for rawspeed_get_number_of_pro...
#include "common/Point.h"                 // for iPoint2D
#include "common/RawImage.h"              // for RawImage, RawImageData
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "io/BitPumpLSB.h"                // for BitPumpLSB
#include "io/Buffer.h"                    // for Buffer, Buffer::size_type
#include "io/Endianness.h"                // for Endianness, Endianness::li...
#include <algorithm>                      // for generate_n, max
#include <cassert>                        // for assert
#include <cstdint>                        // for uint8_t, uint16_t, uint32_t
#include <iterator>                       // for back_insert_iterator, back...
#include <memory>                         // for allocator_traits<>::value_...
#include <utility>                        // for move
#include <vector>                         // for vector

namespace rawspeed {

struct PanasonicDecompressorV5::PacketDsc {
  Buffer::size_type bps;
  int pixelsPerPacket;

  constexpr PacketDsc();
  explicit constexpr PacketDsc(int bps_)
      : bps(bps_),
        pixelsPerPacket(PanasonicDecompressorV5::bitsPerPacket / bps) {
    // NOTE: the division is truncating. There may be some padding bits left.
  }
};

constexpr PanasonicDecompressorV5::PacketDsc
    PanasonicDecompressorV5::TwelveBitPacket =
        PanasonicDecompressorV5::PacketDsc(/*bps=*/12);
constexpr PanasonicDecompressorV5::PacketDsc
    PanasonicDecompressorV5::FourteenBitPacket =
        PanasonicDecompressorV5::PacketDsc(/*bps=*/14);

PanasonicDecompressorV5::PanasonicDecompressorV5(const RawImage& img,
                                                 const ByteStream& input_,
                                                 uint32_t bps_)
    : mRaw(img), bps(bps_) {
  if (mRaw->getCpp() != 1 || mRaw->getDataType() != TYPE_USHORT16 ||
      mRaw->getBpp() != 2)
    ThrowRDE("Unexpected component count / data type");

  const PacketDsc* dsc = nullptr;
  switch (bps) {
  case 12:
    dsc = &TwelveBitPacket;
    break;
  case 14:
    dsc = &FourteenBitPacket;
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
    ThrowRDE("Insufficient count of input blocks for a given image");

  // We only want those blocks we need, no extras.
  input = input_.peekStream(numBlocks, BlockSize);

  chopInputIntoBlocks(*dsc);
}

void PanasonicDecompressorV5::chopInputIntoBlocks(const PacketDsc& dsc) {
  auto pixelToCoordinate = [width = mRaw->dim.x](unsigned pixel) {
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
  std::vector<uint8_t> buf;
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
    input = ByteStream(
        DataBuffer(Buffer(buf.data(), buf.size()), Endianness::little));
    // input.setByteOrder(Endianness::big); // does not seem to matter?!
  }

public:
  explicit ProxyStream(ByteStream block_) : block(std::move(block_)) {}

  ByteStream& getStream() {
    parseBlock();
    return input;
  }
};

template <const PanasonicDecompressorV5::PacketDsc& dsc>
inline void PanasonicDecompressorV5::processPixelPacket(BitPumpLSB* bs, int row,
                                                        int col) const {
  static_assert(dsc.pixelsPerPacket > 0, "dsc should be compile-time const");
  static_assert(dsc.bps > 0 && dsc.bps <= 16, "");

  const Array2DRef<uint16_t> out(mRaw->getU16DataAsUncroppedArray2DRef());

  assert(bs->getFillLevel() == 0);

  for (int p = 0; p < dsc.pixelsPerPacket;) {
    bs->fill();
    for (; bs->getFillLevel() >= dsc.bps; ++p, ++col)
      out(row, col) = bs->getBitsNoFill(dsc.bps);
  }
  bs->skipBitsNoFill(bs->getFillLevel()); // get rid of padding.
}

template <const PanasonicDecompressorV5::PacketDsc& dsc>
void PanasonicDecompressorV5::processBlock(const Block& block) const {
  static_assert(dsc.pixelsPerPacket > 0, "dsc should be compile-time const");
  static_assert(BlockSize % bytesPerPacket == 0, "");

  ProxyStream proxy(block.bs);
  BitPumpLSB bs(proxy.getStream());

  for (int row = block.beginCoord.y; row <= block.endCoord.y; row++) {
    int col = 0;
    // First row may not begin at the first column.
    if (block.beginCoord.y == row)
      col = block.beginCoord.x;

    int endx = mRaw->dim.x;
    // Last row may end before the last column.
    if (block.endCoord.y == row)
      endx = block.endCoord.x;

    assert(col % dsc.pixelsPerPacket == 0);
    assert(endx % dsc.pixelsPerPacket == 0);

    for (; col < endx; col += dsc.pixelsPerPacket)
      processPixelPacket<dsc>(&bs, row, col);
  }
}

template <const PanasonicDecompressorV5::PacketDsc& dsc>
void PanasonicDecompressorV5::decompressInternal() const noexcept {
#ifdef HAVE_OPENMP
#pragma omp parallel for num_threads(rawspeed_get_number_of_processor_cores()) \
    schedule(static) default(none)
#endif
  for (auto block = blocks.cbegin(); block < blocks.cend();
       ++block) { // NOLINT(openmp-exception-escape): we have checked size
                  // already.
    processBlock<dsc>(*block);
  }
}

void PanasonicDecompressorV5::decompress() const noexcept {
  switch (bps) {
  case 12:
    decompressInternal<TwelveBitPacket>();
    break;
  case 14:
    decompressInternal<FourteenBitPacket>();
    break;
  default:
    __builtin_unreachable();
  }
}

} // namespace rawspeed
