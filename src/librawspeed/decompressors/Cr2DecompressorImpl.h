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

#include "common/Array2DRef.h"            // for Array2DRef
#include "common/Point.h"                 // for iPoint2D, iPoint2D::area_type
#include "common/RawImage.h"              // for RawImage, RawImageData
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "decompressors/Cr2Decompressor.h"
#include "io/BitPumpJPEG.h" // for BitPumpJPEG, BitStream<>::...
#include <algorithm>        // for copy_n, min
#include <array>            // for array
#include <cassert>          // for assert
#include <initializer_list> // for initializer_list

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
  const int frameRowStep;
  const int pixelsPerGroup;
  const int groupSize;
  const int cpp;
  const int colsPerGroup;

  constexpr explicit Dsc(
      std::tuple<int /*N_COMP*/, int /*X_S_F*/, int /*Y_S_F*/> format)
      : N_COMP(std::get<0>(format)), X_S_F(std::get<1>(format)),
        Y_S_F(std::get<2>(format)), subSampled(X_S_F != 1 || Y_S_F != 1),
        sliceColStep(N_COMP * X_S_F), frameRowStep(Y_S_F),
        pixelsPerGroup(X_S_F * Y_S_F),
        groupSize(!subSampled ? N_COMP : 2 + pixelsPerGroup),
        cpp(!subSampled ? 1 : 3), colsPerGroup(!subSampled ? cpp : groupSize) {}
};

} // namespace

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

  if (!mRaw->dim.x || !mRaw->dim.y || mRaw->dim.x > 19440 ||
      mRaw->dim.y > 5920) {
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", mRaw->dim.x,
             mRaw->dim.y);
  }

  if (!frame.hasPositiveArea())
    ThrowRDE("LJPEG Frame size is wrong");

  for (auto sliceId = 0; sliceId < slicing.numSlices; sliceId++) {
    const auto sliceWidth = slicing.widthOfSlice(sliceId);
    if (sliceWidth <= 0)
      ThrowRDE("Bad slice width: %i", sliceWidth);
  }

  if (!((std::make_tuple(3, 2, 2) == format) ||
        (std::make_tuple(3, 2, 1) == format) ||
        (std::make_tuple(2, 1, 1) == format) ||
        (std::make_tuple(4, 1, 1) == format)))
    ThrowRDE("Unknown format <%i,%i,%i>", std::get<0>(format),
             std::get<1>(format), std::get<2>(format));

  const Dsc dsc(format);

  if (dsc.subSampled == mRaw->isCFA)
    ThrowRDE("Cannot decode subsampled image to CFA data or vice versa");

  if (static_cast<int>(rec.size()) != dsc.N_COMP)
    ThrowRDE("HT/Initial predictor count does not match component count");

  for (const auto& recip : rec) {
    if (!recip.ht.isFullDecode())
      ThrowRDE("Huffman table is not of a full decoding variety");
  }

  iPoint2D realDim = mRaw->dim;
  if (dsc.subSampled) {
    assert(realDim.x % dsc.groupSize == 0);
    realDim.x /= dsc.groupSize;
    realDim.x *= dsc.X_S_F;
    realDim.y *= dsc.Y_S_F;
  }

  for (auto* width : {&slicing.sliceWidth, &slicing.lastSliceWidth}) {
    if (*width > realDim.x)
      ThrowRDE("Slice is longer than image's height, which is unsupported.");
    if (*width % dsc.sliceColStep != 0) {
      ThrowRDE("Slice width (%u) should be multiple of pixel group size (%u)",
               *width, dsc.sliceColStep);
    }
    if (*width % dsc.cpp != 0) {
      ThrowRDE("Slice width (%u) should be multiple of image cpp (%u)", *width,
               dsc.cpp);
    }
    *width /= dsc.sliceColStep;
    // NOTE: width is no longer guaranteed to be a multiple of cpp!
  }

  if (iPoint2D::area_type(frame.y) * dsc.sliceColStep * slicing.totalWidth() <
      dsc.cpp * realDim.area())
    ThrowRDE("Incorrect slice height / slice widths! Less than image size.");
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

  iPoint2D realDim = mRaw->dim;
  if (dsc.subSampled) {
    assert(realDim.x % dsc.groupSize == 0);
    realDim.x /= dsc.groupSize;
  }

  assert(frame.x % X_S_F == 0);
  assert(frame.y % dsc.frameRowStep == 0);
  const iPoint2D globalFrame(frame.x / X_S_F, frame.y / dsc.frameRowStep);

  auto ht = getHuffmanTables<N_COMP>();
  auto pred = getInitialPreds<N_COMP>();
  const auto* predNext = &out(0, 0);

  BitPumpJPEG bs(input);

  int globalFrameCol = 0;
  int globalFrameRow = 0;
  for (auto sliceId = 0; sliceId < slicing.numSlices; sliceId++) {
    const int sliceWidth = slicing.widthOfSlice(sliceId);

    for (int sliceFrameRow = 0; sliceFrameRow < globalFrame.y;
         ++sliceFrameRow, ++globalFrameRow) {
      int row = globalFrameRow % realDim.y;
      int col = globalFrameRow / realDim.y;
      col *= slicing.widthOfSlice(0);

      if (col >= realDim.x)
        break;

      for (int sliceCol = 0; sliceCol < sliceWidth;) {
        // check if we processed one full raw row worth of pixels
        if (globalFrameCol == globalFrame.x) {
          // if yes -> update predictor by going back exactly one row,
          // no matter where we are right now.
          // makes no sense from an image compression point of view, ask Canon.
          for (int c = 0; c < N_COMP; ++c)
            pred[c] = predNext[c == 0 ? c : dsc.groupSize - (N_COMP - c)];
          predNext = &out(row, dsc.groupSize * col);
          globalFrameCol = 0;
        }

        // How many pixel can we decode until we finish the row of either
        // the frame (i.e. predictor change time), or of the current slice?
        int sliceColsRemainingInThisFrameRow = globalFrame.x - globalFrameCol;
        int sliceColsRemainingInThisSliceRow = sliceWidth - sliceCol;
        int sliceColsRemaining = std::min(sliceColsRemainingInThisSliceRow,
                                          sliceColsRemainingInThisFrameRow);
        for (int sliceColEnd = sliceCol + sliceColsRemaining;
             sliceCol < sliceColEnd; ++sliceCol, ++globalFrameCol, ++col) {
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
