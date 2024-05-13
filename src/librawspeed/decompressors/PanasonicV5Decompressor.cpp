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

#include "rawspeedconfig.h"
#include "decompressors/PanasonicV5Decompressor.h"
#include "adt/Array1DRef.h"
#include "adt/Array2DRef.h"
#include "adt/Casts.h"
#include "adt/Invariant.h"
#include "adt/Point.h"
#include "bitstreams/BitStreamerLSB.h"
#include "common/Common.h"
#include "common/RawImage.h"
#include "decoders/RawDecoderException.h"
#include "io/Buffer.h"
#include "io/ByteStream.h"
#include "io/Endianness.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <utility>
#include <vector>

#ifndef NDEBUG
#include <limits>
#endif

namespace rawspeed {

struct PanasonicV5Decompressor::PacketDsc {
  Buffer::size_type bps;
  int pixelsPerPacket;

  constexpr PacketDsc();
  explicit constexpr PacketDsc(int bps_)
      : bps(bps_),
        pixelsPerPacket(PanasonicV5Decompressor::bitsPerPacket / bps) {
    // NOTE: the division is truncating. There may be some padding bits left.
  }
};

constexpr PanasonicV5Decompressor::PacketDsc
    PanasonicV5Decompressor::TwelveBitPacket =
        PanasonicV5Decompressor::PacketDsc(/*bps=*/12);
constexpr PanasonicV5Decompressor::PacketDsc
    PanasonicV5Decompressor::FourteenBitPacket =
        PanasonicV5Decompressor::PacketDsc(/*bps=*/14);

PanasonicV5Decompressor::PanasonicV5Decompressor(RawImage img,
                                                 ByteStream input_,
                                                 uint32_t bps_)
    : mRaw(std::move(img)), bps(bps_) {
  if (mRaw->getCpp() != 1 || mRaw->getDataType() != RawImageType::UINT16 ||
      mRaw->getBpp() != sizeof(uint16_t))
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
  invariant(mRaw->dim.area() % dsc->pixelsPerPacket == 0);
  const auto numPackets = mRaw->dim.area() / dsc->pixelsPerPacket;
  invariant(numPackets > 0);

  // And how many blocks that would be? Last block may not be full, pad it.
  numBlocks = roundUpDivisionSafe(numPackets, PacketsPerBlock);
  invariant(numBlocks > 0);

  // Does the input contain enough blocks?
  // How many full blocks does the input contain? This is truncating division.
  if (const auto haveBlocks = input_.getRemainSize() / BlockSize;
      haveBlocks < numBlocks)
    ThrowRDE("Insufficient count of input blocks for a given image");

  // We only want those blocks we need, no extras.
  input =
      input_.peekStream(implicit_cast<Buffer::size_type>(numBlocks), BlockSize);

  chopInputIntoBlocks(*dsc);
}

void PanasonicV5Decompressor::chopInputIntoBlocks(const PacketDsc& dsc) {
  auto pixelToCoordinate = [width = mRaw->dim.x](unsigned pixel) {
    return iPoint2D(pixel % width, pixel / width);
  };

  invariant(numBlocks * BlockSize == input.getRemainSize());
  assert(numBlocks <= std::numeric_limits<uint32_t>::max());
  assert(numBlocks <= std::numeric_limits<size_t>::max());
  blocks.reserve(implicit_cast<size_t>(numBlocks));

  const auto pixelsPerBlock = dsc.pixelsPerPacket * PacketsPerBlock;
  invariant((numBlocks - 1U) * pixelsPerBlock < mRaw->dim.area());
  invariant(numBlocks * pixelsPerBlock >= mRaw->dim.area());

  unsigned currPixel = 0;
  std::generate_n(std::back_inserter(blocks), numBlocks,
                  [&, pixelToCoordinate, pixelsPerBlock]() {
                    ByteStream bs = input.getStream(BlockSize);
                    iPoint2D beginCoord = pixelToCoordinate(currPixel);
                    currPixel += pixelsPerBlock;
                    iPoint2D endCoord = pixelToCoordinate(currPixel);
                    return Block(bs, beginCoord, endCoord);
                  });
  assert(blocks.size() == numBlocks);
  invariant(currPixel >= mRaw->dim.area());
  invariant(input.getRemainSize() == 0);

  // Clamp the end coordinate for the last block.
  blocks.back().endCoord = mRaw->dim;
  blocks.back().endCoord.y -= 1;
}

class PanasonicV5Decompressor::ProxyStream {
  ByteStream block;
  std::vector<uint8_t> buf;
  ByteStream input;

  void parseBlock() {
    assert(buf.empty());
    invariant(block.getRemainSize() == BlockSize);

    static_assert(BlockSize > sectionSplitOffset);

    Buffer FirstSection = block.getBuffer(sectionSplitOffset);
    Buffer SecondSection = block.getBuffer(block.getRemainSize());
    invariant(FirstSection.getSize() < SecondSection.getSize());

    buf.reserve(BlockSize);

    // First copy the second section. This makes it the first section.
    buf.insert(buf.end(), SecondSection.begin(), SecondSection.end());
    // Now append the original 1'st section right after the new 1'st section.
    buf.insert(buf.end(), FirstSection.begin(), FirstSection.end());

    assert(buf.size() == BlockSize);
    invariant(block.getRemainSize() == 0);

    // And reset the clock.
    input = ByteStream(DataBuffer(
        Buffer(buf.data(), implicit_cast<Buffer::size_type>(buf.size())),
        Endianness::little));
    // input.setByteOrder(Endianness::big); // does not seem to matter?!
  }

public:
  explicit ProxyStream(ByteStream block_) : block(block_) {}

  ByteStream& getStream() {
    parseBlock();
    return input;
  }
};

template <const PanasonicV5Decompressor::PacketDsc& dsc>
inline void PanasonicV5Decompressor::processPixelPacket(BitStreamerLSB& bs,
                                                        int row,
                                                        int col) const {
  static_assert(dsc.pixelsPerPacket > 0, "dsc should be compile-time const");
  static_assert(dsc.bps > 0 && dsc.bps <= 16);

  const Array2DRef<uint16_t> out(mRaw->getU16DataAsUncroppedArray2DRef());

  invariant(bs.getFillLevel() == 0);

  for (int p = 0; p < dsc.pixelsPerPacket;) {
    bs.fill();
    for (; bs.getFillLevel() >= implicit_cast<int>(dsc.bps); ++p, ++col)
      out(row, col) = implicit_cast<uint16_t>(bs.getBitsNoFill(dsc.bps));
  }
  bs.skipBitsNoFill(bs.getFillLevel()); // get rid of padding.
}

template <const PanasonicV5Decompressor::PacketDsc& dsc>
void PanasonicV5Decompressor::processBlock(const Block& block) const {
  static_assert(dsc.pixelsPerPacket > 0, "dsc should be compile-time const");
  static_assert(BlockSize % bytesPerPacket == 0);

  ProxyStream proxy(block.bs);
  BitStreamerLSB bs(proxy.getStream().peekRemainingBuffer().getAsArray1DRef());

  for (int row = block.beginCoord.y; row <= block.endCoord.y; row++) {
    int col = 0;
    // First row may not begin at the first column.
    if (block.beginCoord.y == row)
      col = block.beginCoord.x;

    int endx = mRaw->dim.x;
    // Last row may end before the last column.
    if (block.endCoord.y == row)
      endx = block.endCoord.x;

    invariant(col % dsc.pixelsPerPacket == 0);
    invariant(endx % dsc.pixelsPerPacket == 0);

    for (; col < endx; col += dsc.pixelsPerPacket)
      processPixelPacket<dsc>(bs, row, col);
  }
}

template <const PanasonicV5Decompressor::PacketDsc& dsc>
void PanasonicV5Decompressor::decompressInternal() const noexcept {
#ifdef HAVE_OPENMP
#pragma omp parallel for num_threads(rawspeed_get_number_of_processor_cores()) \
    schedule(static) default(none)
#endif
  for (const auto& block :
       Array1DRef(blocks.data(), implicit_cast<int>(blocks.size()))) {
    try {
      processBlock<dsc>(block);
    } catch (...) {
      // We should not get any exceptions here.
      __builtin_unreachable();
    }
  }
}

void PanasonicV5Decompressor::decompress() const noexcept {
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
