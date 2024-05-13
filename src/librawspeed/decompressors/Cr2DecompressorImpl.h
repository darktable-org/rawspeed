/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2017 Axel Waggershauser
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

#include "rawspeedconfig.h"
#include "adt/Array1DRef.h"
#include "adt/Array2DRef.h"
#include "adt/Invariant.h"
#include "adt/Optional.h"
#include "adt/Point.h"
#include "adt/iterator_range.h"
#include "bitstreams/BitStreamerJPEG.h"
#include "common/RawImage.h"
#include "decoders/RawDecoderException.h"
#include "decompressors/Cr2Decompressor.h"
#include "io/ByteStream.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <tuple>
#include <utility>
#include <vector>

namespace rawspeed {

class ByteStream;

// This is not really a header, inline namespace is fine.
// NOLINTNEXTLINE(google-build-namespaces)
namespace {

enum class TileSequenceStatus : uint8_t {
  ContinuesColumn,
  BeginsNewColumn,
  Invalid
};

inline TileSequenceStatus
evaluateConsecutiveTiles(const iRectangle2D& rect,
                         const iRectangle2D& nextRect) {
  using enum TileSequenceStatus;
  // Are these two are verically-adjacent rectangles of same width?
  if (rect.getBottomLeft() == nextRect.getTopLeft() &&
      rect.getBottomRight() == nextRect.getTopRight())
    return ContinuesColumn;
  // Otherwise, the next rectangle should be the first row of next column.
  if (nextRect.getTop() == 0 && nextRect.getLeft() == rect.getRight())
    return BeginsNewColumn;
  return Invalid;
}

} // namespace

struct Cr2SliceIterator final {
  int frameHeight;

  Cr2SliceWidthIterator widthIter;

  using iterator_category = std::input_iterator_tag;
  using difference_type = std::ptrdiff_t;
  using value_type = iPoint2D;
  using pointer = const value_type*;   // Unusable, but must be here.
  using reference = const value_type&; // Unusable, but must be here.

  Cr2SliceIterator(const Cr2SliceWidthIterator& sliceWidthIter_,
                   const iPoint2D& frame)
      : frameHeight(frame.y), widthIter(sliceWidthIter_) {}

  value_type RAWSPEED_READONLY operator*() const {
    return {*widthIter, frameHeight};
  }
  Cr2SliceIterator& operator++() {
    ++widthIter;
    return *this;
  }
  friend bool operator==(const Cr2SliceIterator& a, const Cr2SliceIterator& b) {
    invariant(a.frameHeight == b.frameHeight && "Unrelated iterators.");
    return a.widthIter == b.widthIter;
  }
};

struct Cr2OutputTileIterator final {
  const iPoint2D& imgDim;

  Cr2SliceIterator sliceIter;
  iPoint2D outPos = {0, 0};
  int sliceRow = 0;

  using iterator_category = std::input_iterator_tag;
  using difference_type = std::ptrdiff_t;
  using value_type = iRectangle2D;
  using pointer = const value_type*;   // Unusable, but must be here.
  using reference = const value_type&; // Unusable, but must be here.

  Cr2OutputTileIterator(const Cr2SliceIterator& sliceIter_,
                        const iPoint2D& imgDim_)
      : imgDim(imgDim_), sliceIter(sliceIter_) {}

  value_type operator*() const {
    // Positioning
    iRectangle2D tile = {outPos, *sliceIter};
    // Clamping
    int outRowsRemaining = imgDim.y - tile.getTop();
    invariant(outRowsRemaining >= 0);
    int tileRowsRemaining = tile.getHeight() - sliceRow;
    invariant(tileRowsRemaining >= 0);
    const int tileHeight = std::min(outRowsRemaining, tileRowsRemaining);
    tile.dim.y = tileHeight;
    return tile;
  }
  Cr2OutputTileIterator& operator++() {
    const iRectangle2D currTile = operator*();
    sliceRow += currTile.getHeight();
    outPos = currTile.getBottomLeft();
    invariant(sliceRow >= 0 && sliceRow <= (*sliceIter).y && "Overflow");
    if (sliceRow == (*sliceIter).y) {
      ++sliceIter;
      sliceRow = 0;
    }
    if (outPos.y == imgDim.y) {
      outPos.y = 0;
      outPos.x += currTile.getWidth();
    }
    return *this;
  }
  friend bool RAWSPEED_READONLY operator==(const Cr2OutputTileIterator& a,
                                           const Cr2OutputTileIterator& b) {
    invariant(&a.imgDim == &b.imgDim && "Unrelated iterators.");
    // NOTE: outPos is correctly omitted here.
    return a.sliceIter == b.sliceIter && a.sliceRow == b.sliceRow;
  }
};

class Cr2VerticalOutputStripIterator final {
  Cr2OutputTileIterator outputTileIterator;
  Cr2OutputTileIterator outputTileIterator_end;

  [[nodiscard]] std::pair<iRectangle2D, int> coalesce() const {
    Cr2OutputTileIterator tmpIter = outputTileIterator;
    invariant(tmpIter != outputTileIterator_end && "Iterator overflow.");

    iRectangle2D rect = *tmpIter;
    int num = 1;

    for (++tmpIter; tmpIter != outputTileIterator_end; ++tmpIter) {
      iRectangle2D nextRect = *tmpIter;
      const TileSequenceStatus s = evaluateConsecutiveTiles(rect, nextRect);
      invariant(s != TileSequenceStatus::Invalid && "Bad tiling.");
      if (s == TileSequenceStatus::BeginsNewColumn)
        break;
      invariant(s == TileSequenceStatus::ContinuesColumn);
      rect.dim.y += nextRect.dim.y;
      ++num;
    }

    return {rect, num};
  }

public:
  using iterator_category = std::input_iterator_tag;
  using difference_type = std::ptrdiff_t;
  using value_type = iRectangle2D;
  using pointer = const value_type*;   // Unusable, but must be here.
  using reference = const value_type&; // Unusable, but must be here.

  Cr2VerticalOutputStripIterator(
      const Cr2OutputTileIterator& outputTileIterator_,
      const Cr2OutputTileIterator& outputTileIterator_end_)
      : outputTileIterator(outputTileIterator_),
        outputTileIterator_end(outputTileIterator_end_) {}

  value_type operator*() const { return coalesce().first; }
  Cr2VerticalOutputStripIterator& operator++() {
    std::advance(outputTileIterator, coalesce().second);
    return *this;
  }
  friend bool operator==(const Cr2VerticalOutputStripIterator& a,
                         const Cr2VerticalOutputStripIterator& b) {
    invariant(a.outputTileIterator_end == b.outputTileIterator_end &&
              "Comparing unrelated iterators.");
    return a.outputTileIterator == b.outputTileIterator;
  }
};

template <typename PrefixCodeDecoder>
iterator_range<Cr2SliceIterator>
Cr2Decompressor<PrefixCodeDecoder>::getSlices() const {
  return {Cr2SliceIterator(slicing.begin(), frame),
          Cr2SliceIterator(slicing.end(), frame)};
}

template <typename PrefixCodeDecoder>
iterator_range<Cr2OutputTileIterator>
Cr2Decompressor<PrefixCodeDecoder>::getAllOutputTiles() const {
  auto slices = getSlices();
  return {Cr2OutputTileIterator(std::begin(slices), dim),
          Cr2OutputTileIterator(std::end(slices), dim)};
}

template <typename PrefixCodeDecoder>
iterator_range<Cr2OutputTileIterator>
Cr2Decompressor<PrefixCodeDecoder>::getOutputTiles() const {
  auto allOutputTiles = getAllOutputTiles();
  auto first = allOutputTiles.begin();
  auto end = allOutputTiles.end();
  invariant(first != end && "No tiles?");
  auto last = first;
  while (std::next(last) != end && (*last).getBottomRight() != dim)
    ++last;
  assert((*last).getBottomRight() == dim && "Bad tiling");
  return {first, ++last};
}

template <typename PrefixCodeDecoder>
[[nodiscard]] iterator_range<Cr2VerticalOutputStripIterator>
Cr2Decompressor<PrefixCodeDecoder>::getVerticalOutputStrips() const {
  auto outputTiles = getOutputTiles();
  return {Cr2VerticalOutputStripIterator(std::begin(outputTiles),
                                         std::end(outputTiles)),
          Cr2VerticalOutputStripIterator(std::end(outputTiles),
                                         std::end(outputTiles))};
}

// This is not really a header, inline namespace is fine.
// NOLINTNEXTLINE(google-build-namespaces)
namespace {

struct Dsc final {
  const int N_COMP;
  const int X_S_F;
  const int Y_S_F;

  const bool subSampled;

  // inner loop decodes one group of pixels at a time
  //  * for <N,1,1>: N  = N*1*1 (full raw)
  //  * for <3,2,1>: 6  = 3*2*1
  //  * for <3,2,2>: 12 = 3*2*2
  // and advances x by N_COMP*X_S_F and y by Y_S_F
  const int sliceColStep;
  const int pixelsPerGroup;
  const int groupSize;
  const int cpp;
  const int colsPerGroup;

  constexpr explicit Dsc(
      std::tuple<int /*N_COMP*/, int /*X_S_F*/, int /*Y_S_F*/> format)
      : N_COMP(std::get<0>(format)), X_S_F(std::get<1>(format)),
        Y_S_F(std::get<2>(format)), subSampled(X_S_F != 1 || Y_S_F != 1),
        sliceColStep(N_COMP * X_S_F), pixelsPerGroup(X_S_F * Y_S_F),
        groupSize(!subSampled ? N_COMP : 2 + pixelsPerGroup),
        cpp(!subSampled ? 1 : 3), colsPerGroup(!subSampled ? cpp : groupSize) {}
};

} // namespace

template <typename PrefixCodeDecoder>
Cr2Decompressor<PrefixCodeDecoder>::Cr2Decompressor(
    RawImage mRaw_,
    std::tuple<int /*N_COMP*/, int /*X_S_F*/, int /*Y_S_F*/> format_,
    iPoint2D frame_, Cr2SliceWidths slicing_,
    std::vector<PerComponentRecipe> rec_, Array1DRef<const uint8_t> input_)
    : mRaw(std::move(mRaw_)), format(std::move(format_)), frame(frame_),
      slicing(slicing_), rec(std::move(rec_)), input(input_) {
  if (mRaw->getDataType() != RawImageType::UINT16)
    ThrowRDE("Unexpected data type");

  if (mRaw->getCpp() != 1 || mRaw->getBpp() != sizeof(uint16_t))
    ThrowRDE("Unexpected cpp: %u", mRaw->getCpp());

  if (!((std::make_tuple(3, 2, 2) == format) ||
        (std::make_tuple(3, 2, 1) == format) ||
        (std::make_tuple(2, 1, 1) == format) ||
        (std::make_tuple(4, 1, 1) == format)))
    ThrowRDE("Unknown format <%i,%i,%i>", std::get<0>(format),
             std::get<1>(format), std::get<2>(format));

  const Dsc dsc(format);

  dim = mRaw->dim;
  if (!dim.hasPositiveArea() || dim.x % dsc.groupSize != 0)
    ThrowRDE("Unexpected image dimension multiplicity");
  dim.x /= dsc.groupSize;

  if (!frame.hasPositiveArea() || frame.x % dsc.X_S_F != 0 ||
      frame.y % dsc.Y_S_F != 0)
    ThrowRDE("Unexpected LJpeg frame dimension multiplicity");
  frame.x /= dsc.X_S_F;
  frame.y /= dsc.Y_S_F;

  if (mRaw->dim.x > 19440 || mRaw->dim.y > 5920) {
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", mRaw->dim.x,
             mRaw->dim.y);
  }

  for (auto sliceId = 0; sliceId < slicing.numSlices; sliceId++) {
    const auto sliceWidth = slicing.widthOfSlice(sliceId);
    if (sliceWidth <= 0)
      ThrowRDE("Bad slice width: %i", sliceWidth);
  }

  if (dsc.subSampled == mRaw->isCFA)
    ThrowRDE("Cannot decode subsampled image to CFA data or vice versa");

  if (static_cast<int>(rec.size()) != dsc.N_COMP)
    ThrowRDE("HT/Initial predictor count does not match component count");

  for (const auto& recip : rec) {
    if (!recip.ht.isFullDecode())
      ThrowRDE("Huffman table is not of a full decoding variety");
  }

  for (auto* width : {&slicing.sliceWidth, &slicing.lastSliceWidth}) {
    if (*width % dsc.sliceColStep != 0) {
      ThrowRDE("Slice width (%u) should be multiple of pixel group size (%u)",
               *width, dsc.sliceColStep);
    }
    *width /= dsc.sliceColStep;
  }

  if (frame.area() < dim.area())
    ThrowRDE("Frame area smaller than the image area");

  Optional<iRectangle2D> lastTile;
  for (iRectangle2D output : getAllOutputTiles()) {
    if (lastTile && evaluateConsecutiveTiles(*lastTile, output) ==
                        TileSequenceStatus::Invalid)
      ThrowRDE("Invalid tiling - slice width change mid-output row?");
    if (output.getBottomRight() <= dim) {
      lastTile = output;
      continue; // Tile still inbounds of image.
    }
    if (output.getTopLeft() < dim)
      ThrowRDE("Output tile partially outside of image");
    break; // Skip the rest of the tiles - they do not contribute to the image.
  }
  if (!lastTile)
    ThrowRDE("No tiles are provided");
  if (lastTile->getBottomRight() != dim)
    ThrowRDE("Tiles do not cover the entire image area.");
}

template <typename PrefixCodeDecoder>
template <int N_COMP, size_t... I>
std::array<std::reference_wrapper<const PrefixCodeDecoder>, N_COMP>
Cr2Decompressor<PrefixCodeDecoder>::getPrefixCodeDecodersImpl(
    std::index_sequence<I...> /*unused*/) const {
  return std::array<std::reference_wrapper<const PrefixCodeDecoder>, N_COMP>{
      std::cref(rec[I].ht)...};
}

template <typename PrefixCodeDecoder>
template <int N_COMP>
std::array<std::reference_wrapper<const PrefixCodeDecoder>, N_COMP>
Cr2Decompressor<PrefixCodeDecoder>::getPrefixCodeDecoders() const {
  return getPrefixCodeDecodersImpl<N_COMP>(std::make_index_sequence<N_COMP>{});
}

template <typename PrefixCodeDecoder>
template <int N_COMP>
std::array<uint16_t, N_COMP>
Cr2Decompressor<PrefixCodeDecoder>::getInitialPreds() const {
  std::array<uint16_t, N_COMP> preds;
  std::transform(
      rec.begin(), rec.end(), preds.begin(),
      [](const PerComponentRecipe& compRec) { return compRec.initPred; });
  return preds;
}

// N_COMP == number of components (2, 3 or 4)
// X_S_F  == x/horizontal sampling factor (1 or 2)
// Y_S_F  == y/vertical   sampling factor (1 or 2)

template <typename PrefixCodeDecoder>
template <int N_COMP, int X_S_F, int Y_S_F>
ByteStream::size_type
Cr2Decompressor<PrefixCodeDecoder>::decompressN_X_Y() const {
  const Array2DRef<uint16_t> out(mRaw->getU16DataAsUncroppedArray2DRef());

  // To understand the CR2 slice handling and sampling factor behavior, see
  // https://github.com/lclevy/libcraw2/blob/master/docs/cr2_lossless.pdf?raw=true

  constexpr Dsc dsc({N_COMP, X_S_F, Y_S_F});

  // inner loop decodes one group of pixels at a time
  //  * for <N,1,1>: N  = N*1*1 (full raw)
  //  * for <3,2,1>: 6  = 3*2*1
  //  * for <3,2,2>: 12 = 3*2*2
  // and advances x by N_COMP*X_S_F and y by Y_S_F

  auto ht = getPrefixCodeDecoders<N_COMP>();
  auto pred = getInitialPreds<N_COMP>();
  auto predNext = out[/*row=*/0]
                      .getCrop(/*offset=*/0, /*size=*/dsc.groupSize)
                      .getAsArray1DRef();

  BitStreamerJPEG bs(input);

  int globalFrameCol = 0;
  int globalFrameRow = 0;
  (void)globalFrameRow;

  auto frameColsRemaining = [&]() {
    int r = frame.x - globalFrameCol;
    invariant(r >= 0);
    return r;
  };

  for (iRectangle2D output : getVerticalOutputStrips()) {
    for (int row = output.getTop(), rowEnd = output.getBottom(); row != rowEnd;
         ++row) {
      for (int col = output.getLeft(), colEnd = output.getRight();
           col != colEnd;) {
        // check if we processed one full raw row worth of pixels
        if (frameColsRemaining() == 0) {
          // if yes -> update predictor by going back exactly one row,
          // no matter where we are right now.
          // makes no sense from an image compression point of view, ask
          // Canon.
          for (int c = 0; c < N_COMP; ++c)
            pred[c] = predNext(c == 0 ? c : dsc.groupSize - (N_COMP - c));
          predNext = out[row]
                         .getBlock(/*size=*/dsc.groupSize,
                                   /*index=*/col)
                         .getAsArray1DRef();
          ++globalFrameRow;
          globalFrameCol = 0;
          invariant(globalFrameRow < frame.y && "Run out of frame");
        }

        // How many pixel can we decode until we finish the row of either
        // the frame (i.e. predictor change time), or of the current slice?
        for (int colFrameEnd = std::min(colEnd, col + frameColsRemaining());
             col != colFrameEnd; ++col, ++globalFrameCol) {
          for (int p = 0; p < dsc.groupSize; ++p) {
            int c = p < dsc.pixelsPerGroup ? 0 : p - dsc.pixelsPerGroup + 1;
            out(row, dsc.groupSize * col + p) = pred[c] +=
                (static_cast<const PrefixCodeDecoder&>(ht[c]))
                    .decodeDifference(bs);
          }
        }
      }
    }
  }
  return bs.getStreamPosition();
}

template <typename PrefixCodeDecoder>
ByteStream::size_type Cr2Decompressor<PrefixCodeDecoder>::decompress() const {
  if (std::make_tuple(3, 2, 2) == format) {
    return decompressN_X_Y<3, 2, 2>(); // Cr2 sRaw1/mRaw
  }
  if (std::make_tuple(3, 2, 1) == format) {
    return decompressN_X_Y<3, 2, 1>(); // Cr2 sRaw2/sRaw
  }
  if (std::make_tuple(2, 1, 1) == format) {
    return decompressN_X_Y<2, 1, 1>();
  }
  if (std::make_tuple(4, 1, 1) == format) {
    return decompressN_X_Y<4, 1, 1>();
  }
  __builtin_unreachable();
}

} // namespace rawspeed
