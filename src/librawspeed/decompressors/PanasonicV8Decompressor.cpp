/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2022-2024 LibRaw LLC (info@libraw.org)
    Copyright (C) 2024 Daniel Vogelbacher
    Copyright (C) 2025 Kolton Yager

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include "rawspeedconfig.h"
#include "decompressors/PanasonicV8Decompressor.h"
#include "adt/Array1DRef.h"
#include "adt/Array1DRefExtras.h"
#include "adt/Array2DRef.h"
#include "adt/CroppedArray2DRef.h"
#include "adt/Invariant.h"
#include "adt/Point.h"
#include "adt/TiledArray2DRef.h"
#include "bitstreams/BitStream.h"
#include "bitstreams/BitStreamer.h"
#include "bitstreams/BitStreamerMSB.h" // IWYU pragma: keep
#include "bitstreams/BitStreams.h"
#include "common/Common.h"
#include "common/RawImage.h"
#include "common/RawspeedException.h"
#include "decoders/RawDecoderException.h"
#include "io/IOException.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace rawspeed {

// The set of templates and classes below define a specialized bit streamer
// which works mostly the same as BitStreamerMSB, but which reverses the
// ordering of the bits within each byte.
template <typename Tag>
struct BitStreamerReversedSequentialReplenisher
    : public BitStreamerForwardSequentialReplenisher<Tag> {
  using Base = BitStreamerForwardSequentialReplenisher<Tag>;
  using Traits = BitStreamerTraits<Tag>;
  using StreamTraits = BitStreamTraits<Traits::Tag>;

  using Base::BitStreamerForwardSequentialReplenisher;

  // Almost an exact copy of
  // BitStreamerForwardSequentialReplenisher::getInput(), but here we flip the
  // order of the bits within each byte, as they get loaded.
  std::array<std::byte, BitStreamerTraits<Tag>::MaxProcessBytes> getInput() {
    Base::establishClassInvariants();

    std::array<std::byte, BitStreamerTraits<Tag>::MaxProcessBytes> tmpStorage;
    auto tmp = Array1DRef<std::byte>(tmpStorage.data(),
                                     implicit_cast<int>(tmpStorage.size()));

    if (Base::getPos() + BitStreamerTraits<Tag>::MaxProcessBytes <=
        Base::input.size()) [[likely]] {
      auto currInput =
          Base::input
              .getCrop(Base::getPos(), BitStreamerTraits<Tag>::MaxProcessBytes)
              .getAsArray1DRef();
      invariant(currInput.size() == tmp.size());
      std::copy_n(currInput.begin(), BitStreamerTraits<Tag>::MaxProcessBytes,
                  tmp.begin());

      // Reverse the order of bits within each byte using a bit-twiddle trick.
      // Three operation bit reversal from:
      // https://graphics.stanford.edu/~seander/bithacks.html#ReverseByteWith64BitsDiv
      for (std::byte& b : tmp) {
        b = std::byte{
            uint8_t((uint8_t(b) * 0x0202020202ULL & 0x010884422010ULL) % 1023)};
      }

      return tmpStorage;
    }

    if (Base::getPos() >
        Base::input.size() + 2 * BitStreamerTraits<Tag>::MaxProcessBytes)
        [[unlikely]]
      ThrowIOE("Buffer overflow read in BitStreamer");

    variableLengthLoadNaiveViaMemcpy(tmp, Base::input, Base::getPos());

    return tmpStorage;
  }
};

class BitStreamerRevMSB;

template <> struct BitStreamerTraits<BitStreamerRevMSB> final {
  static constexpr BitOrder Tag = BitOrder::MSB;

  static constexpr bool canUseWithPrefixCodeDecoder = true;

  static constexpr int MaxProcessBytes = 4;
  static_assert(MaxProcessBytes == sizeof(uint32_t));
};

/// Variation of standard MSB bit streamer. Bits are processed in reverse order
/// (least significant bits consume first).
class BitStreamerRevMSB final
    : public BitStreamer<
          BitStreamerRevMSB,
          BitStreamerReversedSequentialReplenisher<BitStreamerRevMSB>> {
  using Base =
      BitStreamer<BitStreamerRevMSB,
                  BitStreamerReversedSequentialReplenisher<BitStreamerRevMSB>>;

public:
  using Base::Base;
};

namespace {

enum class TileSequenceStatus : uint8_t { ContinuesRow, BeginsNewRow, Invalid };

inline TileSequenceStatus
evaluateConsecutiveTiles(const iRectangle2D rect, const iRectangle2D nextRect) {
  using enum TileSequenceStatus;
  // Are these two are horizontally-adjacent rectangles of same height?
  if (rect.getTopRight() == nextRect.getTopLeft() &&
      rect.getBottomRight() == nextRect.getBottomLeft())
    return ContinuesRow;
  // Otherwise, the next rectangle should be the first row of next Row.
  if (nextRect.getTopLeft() == iPoint2D(0, rect.getBottom()))
    return BeginsNewRow;
  return Invalid;
}

void isValidImageGrid(iRectangle2D imgDim,
                      Array1DRef<const iRectangle2D> rects) {
  auto outPos = imgDim.pos;
  invariant(outPos.x == 0 && outPos.y == 0);

  iRectangle2D rect = rects(0);
  if (rect.pos != outPos)
    ThrowRDE("First tile is out-of-order");
  invariant(rect.isThisInside(imgDim));
  invariant(rect.hasPositiveArea());
  outPos.x += rect.getWidth();
  for (int tileIdx = 1; tileIdx != rects.size(); ++tileIdx) {
    iRectangle2D nextRect = rects(tileIdx);
    invariant(nextRect.isThisInside(imgDim));
    invariant(nextRect.hasPositiveArea());
    switch (evaluateConsecutiveTiles(rect, nextRect)) {
    case TileSequenceStatus::ContinuesRow:
      outPos.x += nextRect.getWidth();
      rect = nextRect;
      continue;
    case TileSequenceStatus::BeginsNewRow:
      if (outPos.x != imgDim.getRight())
        ThrowRDE("Previous row has not been fully filled yet");
      outPos.x = 0;
      outPos.y += nextRect.getHeight();
      rect = nextRect;
      continue;
    case TileSequenceStatus::Invalid:
      ThrowRDE("Invalid tiling config");
    }
  }
  if (rect.getBottomRight() != imgDim.getBottomRight())
    ThrowRDE("Tiles do not cover whole output image");
}

} // namespace

std::vector<iRectangle2D>
PanasonicV8Decompressor::DecompressorParamsBuilder::getOutRects(
    iRectangle2D imgDim, Array1DRef<const uint32_t> stripLineOffsets,
    Array1DRef<const uint16_t> stripWidths,
    Array1DRef<const uint16_t> stripHeights) {
  if (!imgDim.hasPositiveArea())
    ThrowRDE("Empty image requested");
  const int totalStrips = stripLineOffsets.size();
  if (stripWidths.size() != totalStrips || stripHeights.size() != totalStrips)
    ThrowRDE("Inputs have mismatched length");
  if (totalStrips <= 0)
    ThrowRDE("No strips provided");

  std::vector<iRectangle2D> mOutRects;

  for (int stripIdx = 0; stripIdx < totalStrips; ++stripIdx) {
    const uint32_t stripWidth = stripWidths(stripIdx);
    const uint32_t stripHeight = stripHeights(stripIdx);
    const uint32_t stripOutputX = stripLineOffsets(stripIdx) & 0xFFFF;
    const uint32_t stripOutputY = stripLineOffsets(stripIdx) >> 16;

    const auto rect = iRectangle2D(iPoint2D(stripOutputX, stripOutputY),
                                   iPoint2D(stripWidth, stripHeight));

    if (!rect.isThisInside(imgDim))
      ThrowRDE("Tile isn't fully within the output image");
    if (!rect.hasPositiveArea())
      ThrowRDE("The tile is empty");

    mOutRects.emplace_back(rect);
  }

  isValidImageGrid(imgDim, getAsArray1DRef(mOutRects));
  return mOutRects;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

PanasonicV8Decompressor::PanasonicV8Decompressor(
    RawImage outputImg, DecompressorParams mParams_,
    PrefixCodeDecoder mCodeDecoder_)
    : mRawOutput(std::move(outputImg)), mParams(std::move(mParams_)),
      mCodeDecoder(std::move(mCodeDecoder_)) {
  if (mRawOutput->getCpp() != 1 ||
      mRawOutput->getDataType() != RawImageType::UINT16 ||
      mRawOutput->getBpp() != sizeof(uint16_t)) {
    ThrowRDE("Unexpected component count / data type");
  }
  if (!mRawOutput->dim.hasPositiveArea())
    ThrowRDE("Unexpected image dimensions");
}

void PanasonicV8Decompressor::decompress() const {
  const int numStrips = mParams.mStrips.size();
#ifdef HAVE_OPENMP
  unsigned threadCount =
      std::min(numStrips, rawspeed_get_number_of_processor_cores());
#pragma omp parallel for num_threads(threadCount)                              \
    schedule(static) default(none) firstprivate(numStrips)
#endif
  for (int stripIdx = 0; stripIdx < numStrips; ++stripIdx) {
    try {
      Array1DRef<const uint8_t> strip = mParams.mStrips(stripIdx);

      const auto outRect = mParams.mOutRect(stripIdx);

      const auto out = CroppedArray2DRef<uint16_t>(
                           mRawOutput->getU16DataAsUncroppedArray2DRef(),
                           /*offsetCols=*/outRect.pos.x,
                           /*offsetRows=*/outRect.pos.y,
                           /*croppedWidth=*/outRect.dim.x,
                           /*croppedHeight=*/outRect.dim.y)
                           .getAsArray2DRef();

      decompressStrip(out, strip);
    } catch (const RawspeedException& err) {
      // Propagate the exception out of OpenMP magic.
      mRawOutput->setError(err.what());
    } catch (...) {
      // We should not get any other exception type here.
      __builtin_unreachable();
    }
  }
}

void PanasonicV8Decompressor::decompressStrip(
    const Array2DRef<uint16_t> out, Array1DRef<const uint8_t> strip) const {
  BitStreamerRevMSB bs(strip);

  Bayer2x2 predictedStorage = mParams.initialPrediction;
  const auto pred = Array2DRef(predictedStorage.data(), 2, 2);

  invariant(out.height() % 2 == 0);
  invariant(out.width() % 2 == 0);

  for (int j = 0; j != 2; ++j)
    for (int i = 0; i != 2; ++i)
      pred(i, j) = pred(j, i);

  const auto rowGroups = TiledArray2DRef(out,
                                         /*tileWidth=*/out.width(),
                                         /*tileHeight_=*/2);

  invariant(rowGroups.numCols() == 1);
  for (int rowGroup = 0; rowGroup != rowGroups.numRows(); ++rowGroup) {
    const auto outRow = rowGroups(rowGroup, 0).getAsArray2DRef();

    const auto outBlocks = TiledArray2DRef(outRow,
                                           /*tileWidth=*/2,
                                           /*tileHeight=*/2);

    // Each decoded 'row' is actually two rows of pixels in the raw image
    // because the image is encoded in rows of 2x2 CFA tiles. Likewise the
    // effective width here is 2x the strip width.
    invariant(outBlocks.numRows() == 1);
    for (int blockIdx = 0; blockIdx < outBlocks.numCols(); ++blockIdx) {
      const auto outBlock = outBlocks(0, blockIdx).getAsArray2DRef();

      for (int j = 0; j != 2; ++j) {
        for (int i = 0; i != 2; ++i) {
          const int32_t diff = mCodeDecoder.decodeDifference(bs);
          const int32_t decodedValue = pred(i, j) + diff;
          invariant(decodedValue > 0);
          pred(i, j) = uint16_t(std::clamp(
              decodedValue, 0, int32_t(std::numeric_limits<uint16_t>::max())));
          outBlock(i, j) = pred(i, j);
        }
      }
    }

    // At the end of the line, reset predicted value to the first tile of the
    // prior line.
    const auto tmp = outBlocks(0, 0).getAsArray2DRef();
    for (int j = 0; j != 2; ++j)
      for (int i = 0; i != 2; ++i)
        pred(i, j) = tmp(i, j);
  }
}

} // namespace rawspeed
