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

#include "common/Array2DRef.h"               // for Array2DRef
#include "common/Point.h"                    // for iPoint2D, iPoint2D::area_...
#include "common/RawImage.h"                 // for RawImage, RawImageData
#include "common/iterator_range.h"           // for iterator_range
#include "decoders/RawDecoderException.h"    // for ThrowException, ThrowRDE
#include "decompressors/Cr2Decompressor.h"   // for Cr2Decompressor, Cr2Slicing
#include "decompressors/DummyHuffmanTable.h" // for DummyHuffmanTable
#include "decompressors/HuffmanTableLUT.h"   // for HuffmanTableLUT
#include "io/BitPumpJPEG.h"                  // for BitPumpJPEG, BitStream<>:...
#include "io/ByteStream.h"                   // for ByteStream
#include <algorithm>                         // for min, transform
#include <array>                             // for array
#include <cassert>                           // for assert
#include <cstddef>                           // for size_t
#include <cstdint>                           // for uint16_t
#include <functional>                        // for cref, reference_wrapper
#include <initializer_list>                  // for initializer_list
#include <optional>                          // for optional
#include <tuple>                             // for make_tuple, operator==, get
#include <utility>                           // for move, index_sequence, mak...
#include <vector>                            // for vector

namespace rawspeed {

class ByteStream;

// NOLINTNEXTLINE: this is not really a header, inline namespace is fine.
namespace {

struct Dsc {
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

class Cr2OutputTileIterator final {
  const Cr2Slicing& slicing;
  const iPoint2D& frame;
  const iPoint2D& dim;

  int integratedFrameRow;

public:
  using iterator_category = std::input_iterator_tag;
  using difference_type = std::ptrdiff_t;
  using value_type = iRectangle2D;

  Cr2OutputTileIterator(const Cr2Slicing& slicing_, const iPoint2D& frame_,
                        const iPoint2D& dim_, int integratedFrameRow_)
      : slicing(slicing_), frame(frame_), dim(dim_),
        integratedFrameRow(integratedFrameRow_) {}

  value_type operator*() const {
    int sliceRow = integratedFrameRow % frame.y;
    int sliceId = integratedFrameRow / frame.y;
    iRectangle2D r;
    int row = integratedFrameRow % dim.y;
    int col = integratedFrameRow / dim.y;
    col *= slicing.widthOfSlice(0);
    r.setTopLeft({col, row});
    int rowsRemaining = dim.y - row;
    assert(rowsRemaining >= 0);
    int sliceRowsRemaining = frame.y - sliceRow;
    assert(sliceRowsRemaining >= 0);
    const int sliceHeight = std::min(rowsRemaining, sliceRowsRemaining);
    r.setSize({slicing.widthOfSlice(sliceId), sliceHeight});
    return r;
  }
  Cr2OutputTileIterator& operator++() {
    integratedFrameRow += operator*().getHeight();
    return *this;
  }
  friend bool operator==(const Cr2OutputTileIterator& a,
                         const Cr2OutputTileIterator& b) {
    assert(&a.slicing == &b.slicing && &a.frame == &b.frame &&
           &a.dim == &b.dim && "Comparing unrelated iterators.");
    return a.integratedFrameRow == b.integratedFrameRow;
  }
  friend bool operator!=(const Cr2OutputTileIterator& a,
                         const Cr2OutputTileIterator& b) {
    return !(a == b);
  }
};

template <typename HuffmanTable>
Cr2Decompressor<HuffmanTable>::Cr2Decompressor(
    const RawImage& mRaw_,
    std::tuple<int /*N_COMP*/, int /*X_S_F*/, int /*Y_S_F*/> format_,
    iPoint2D frame_, Cr2Slicing slicing_, std::vector<PerComponentRecipe> rec_,
    ByteStream input_)
    : mRaw(mRaw_), format(std::move(format_)), frame(frame_), slicing(slicing_),
      rec(std::move(rec_)), input(std::move(input_)) {
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

  const iRectangle2D fullImage({0, 0}, dim);
  std::optional<iRectangle2D> lastTile;
  for (iRectangle2D output : getOutputTiles()) {
    if (output.getLeft() == dim.x)
      break;
    lastTile = output;
    if (!output.isThisInside(fullImage))
      ThrowRDE("Output tile not inside of the image");
  }
  assert(lastTile && "No tiles?");
  if (lastTile->getBottomRight() != fullImage.getBottomRight())
    ThrowRDE("Tiles do not cover the entire image area.");
}

template <typename HuffmanTable>
template <int N_COMP, size_t... I>
std::array<std::reference_wrapper<const HuffmanTable>, N_COMP>
Cr2Decompressor<HuffmanTable>::getHuffmanTablesImpl(
    std::index_sequence<I...> /*unused*/) const {
  return std::array<std::reference_wrapper<const HuffmanTable>, N_COMP>{
      std::cref(rec[I].ht)...};
}

template <typename HuffmanTable>
template <int N_COMP>
std::array<std::reference_wrapper<const HuffmanTable>, N_COMP>
Cr2Decompressor<HuffmanTable>::getHuffmanTables() const {
  return getHuffmanTablesImpl<N_COMP>(std::make_index_sequence<N_COMP>{});
}

template <typename HuffmanTable>
template <int N_COMP>
std::array<uint16_t, N_COMP>
Cr2Decompressor<HuffmanTable>::getInitialPreds() const {
  std::array<uint16_t, N_COMP> preds;
  std::transform(
      rec.begin(), rec.end(), preds.begin(),
      [](const PerComponentRecipe& compRec) { return compRec.initPred; });
  return preds;
}

template <typename HuffmanTable>
[[nodiscard]] iterator_range<Cr2OutputTileIterator>
Cr2Decompressor<HuffmanTable>::getOutputTiles() {
  return make_range(Cr2OutputTileIterator(slicing, frame, dim,
                                          /*integratedFrameRow=*/0),
                    Cr2OutputTileIterator(
                        slicing, frame, dim,
                        /*integratedFrameRow=*/slicing.numSlices * frame.y));
}

// N_COMP == number of components (2, 3 or 4)
// X_S_F  == x/horizontal sampling factor (1 or 2)
// Y_S_F  == y/vertical   sampling factor (1 or 2)

template <typename HuffmanTable>
template <int N_COMP, int X_S_F, int Y_S_F>
void Cr2Decompressor<HuffmanTable>::decompressN_X_Y() {
  const Array2DRef<uint16_t> out(mRaw->getU16DataAsUncroppedArray2DRef());

  // To understand the CR2 slice handling and sampling factor behavior, see
  // https://github.com/lclevy/libcraw2/blob/master/docs/cr2_lossless.pdf?raw=true

  constexpr Dsc dsc({N_COMP, X_S_F, Y_S_F});

  // inner loop decodes one group of pixels at a time
  //  * for <N,1,1>: N  = N*1*1 (full raw)
  //  * for <3,2,1>: 6  = 3*2*1
  //  * for <3,2,2>: 12 = 3*2*2
  // and advances x by N_COMP*X_S_F and y by Y_S_F

  auto ht = getHuffmanTables<N_COMP>();
  auto pred = getInitialPreds<N_COMP>();
  const auto* predNext = &out(0, 0);

  BitPumpJPEG bs(input);

  int globalFrameCol = 0;
  int globalFrameRow = 0;
  (void)globalFrameRow;

  auto frameColsRemaining = [&]() {
    int r = frame.x - globalFrameCol;
    assert(r >= 0);
    return r;
  };

  for (iRectangle2D output : getOutputTiles()) {
    if (output.getLeft() == dim.x)
      return;
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
            pred[c] = predNext[c == 0 ? c : dsc.groupSize - (N_COMP - c)];
          predNext = &out(row, dsc.groupSize * col);
          ++globalFrameRow;
          globalFrameCol = 0;
          assert(globalFrameRow < frame.y && "Run out of frame");
        }

        // How many pixel can we decode until we finish the row of either
        // the frame (i.e. predictor change time), or of the current slice?
        for (int colFrameEnd = std::min(colEnd, col + frameColsRemaining());
             col != colFrameEnd; ++col, ++globalFrameCol) {
          for (int p = 0; p < dsc.groupSize; ++p) {
            int c = p < dsc.pixelsPerGroup ? 0 : p - dsc.pixelsPerGroup + 1;
            out(row, dsc.groupSize * col + p) = pred[c] +=
                ((const HuffmanTable&)(ht[c])).decodeDifference(bs);
          }
        }
      }
    }
  }
}

template <typename HuffmanTable>
void Cr2Decompressor<HuffmanTable>::decompress() {
  if (std::make_tuple(3, 2, 2) == format) {
    decompressN_X_Y<3, 2, 2>(); // Cr2 sRaw1/mRaw
    return;
  }
  if (std::make_tuple(3, 2, 1) == format) {
    decompressN_X_Y<3, 2, 1>(); // Cr2 sRaw2/sRaw
    return;
  }
  if (std::make_tuple(2, 1, 1) == format) {
    decompressN_X_Y<2, 1, 1>();
    return;
  }
  if (std::make_tuple(4, 1, 1) == format) {
    decompressN_X_Y<4, 1, 1>();
    return;
  }
  __builtin_unreachable();
}

} // namespace rawspeed
