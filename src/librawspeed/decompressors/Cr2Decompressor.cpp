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

#include "decompressors/Cr2Decompressor.h"
#include "common/Array2DRef.h"            // for Array2DRef
#include "common/Point.h"                 // for iPoint2D, iPoint2D::area_type
#include "common/RawImage.h"              // for RawImage, RawImageData
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "io/BitPumpJPEG.h"               // for BitPumpJPEG, BitStream<>::...
#include <algorithm>                      // for copy_n, min
#include <array>                          // for array
#include <cassert>                        // for assert
#include <initializer_list>               // for initializer_list

namespace rawspeed {

class ByteStream;

Cr2Decompressor::Cr2Decompressor(
    const RawImage& mRaw_,
    std::tuple<int /*N_COMP*/, int /*X_S_F*/, int /*Y_S_F*/> format_,
    iPoint2D frame_, Cr2Slicing slicing_, std::vector<const HuffmanTable*> ht_,
    std::vector<uint16_t> initPred_, ByteStream input_)
    : mRaw(mRaw_), format(std::move(format_)), frame(frame_), slicing(slicing_),
      ht(std::move(ht_)), initPred(std::move(initPred_)),
      input(std::move(input_)) {
  if (mRaw->getDataType() != RawImageType::UINT16)
    ThrowRDE("Unexpected data type");

  if (mRaw->getCpp() != 1 || mRaw->getBpp() != sizeof(uint16_t))
    ThrowRDE("Unexpected cpp: %u", mRaw->getCpp());

  if (!mRaw->dim.x || !mRaw->dim.y || mRaw->dim.x > 19440 ||
      mRaw->dim.y > 5920) {
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", mRaw->dim.x,
             mRaw->dim.y);
  }

  for (auto sliceId = 0; sliceId < slicing.numSlices; sliceId++) {
    const auto sliceWidth = slicing.widthOfSlice(sliceId);
    if (sliceWidth <= 0)
      ThrowRDE("Bad slice width: %i", sliceWidth);
  }

  const bool isSubSampled =
      std::get<1>(format) != 1 || std::get<2>(format) != 1;
  if (isSubSampled == mRaw->isCFA)
    ThrowRDE("Cannot decode subsampled image to CFA data or vice versa");

  if (!((std::make_tuple(3, 2, 2) == format) ||
        (std::make_tuple(3, 2, 1) == format) ||
        (std::make_tuple(2, 1, 1) == format) ||
        (std::make_tuple(4, 1, 1) == format)))
    ThrowRDE("Unknown format <%i,%i,%i>", std::get<0>(format),
             std::get<1>(format), std::get<2>(format));

  if (static_cast<int>(initPred.size()) != std::get<0>(format))
    ThrowRDE("Initial predictor count does not match component count");
}

// N_COMP == number of components (2, 3 or 4)
// X_S_F  == x/horizontal sampling factor (1 or 2)
// Y_S_F  == y/vertical   sampling factor (1 or 2)

template <int N_COMP, int X_S_F, int Y_S_F>
void Cr2Decompressor::decompressN_X_Y() {
  const Array2DRef<uint16_t> out(mRaw->getU16DataAsUncroppedArray2DRef());

  // To understand the CR2 slice handling and sampling factor behavior, see
  // https://github.com/lclevy/libcraw2/blob/master/docs/cr2_lossless.pdf?raw=true

  constexpr bool subSampled = X_S_F != 1 || Y_S_F != 1;

  // inner loop decodes one group of pixels at a time
  //  * for <N,1,1>: N  = N*1*1 (full raw)
  //  * for <3,2,1>: 6  = 3*2*1
  //  * for <3,2,2>: 12 = 3*2*2
  // and advances x by N_COMP*X_S_F and y by Y_S_F
  constexpr int sliceColStep = N_COMP * X_S_F;
  constexpr int frameRowStep = Y_S_F;
  constexpr int pixelsPerGroup = X_S_F * Y_S_F;
  constexpr int groupSize = !subSampled ? N_COMP : 2 + pixelsPerGroup;
  const int cpp = !subSampled ? 1 : 3;
  const int colsPerGroup = !subSampled ? cpp : groupSize;

  iPoint2D realDim = mRaw->dim;
  if (subSampled) {
    assert(realDim.x % groupSize == 0);
    realDim.x /= groupSize;
  }
  realDim.x *= X_S_F;
  realDim.y *= Y_S_F;

  auto pred = to_array<N_COMP>(initPred);
  const auto* predNext = &out(0, 0);

  BitPumpJPEG bs(input);

  for (const auto& width : {slicing.sliceWidth, slicing.lastSliceWidth}) {
    if (width > realDim.x)
      ThrowRDE("Slice is longer than image's height, which is unsupported.");
    if (width % sliceColStep != 0) {
      ThrowRDE("Slice width (%u) should be multiple of pixel group size (%u)",
               width, sliceColStep);
    }
    if (width % cpp != 0) {
      ThrowRDE("Slice width (%u) should be multiple of image cpp (%u)", width,
               cpp);
    }
  }

  if (iPoint2D::area_type((unsigned)frame.y) * slicing.totalWidth() <
      cpp * realDim.area())
    ThrowRDE("Incorrect slice height / slice widths! Less than image size.");

  unsigned globalFrameCol = 0;
  unsigned globalFrameRow = 0;
  for (auto sliceId = 0; sliceId < slicing.numSlices; sliceId++) {
    const unsigned sliceWidth = slicing.widthOfSlice(sliceId);

    assert((unsigned)frame.y % frameRowStep == 0);
    for (unsigned sliceFrameRow = 0; sliceFrameRow < (unsigned)frame.y;
         sliceFrameRow += frameRowStep, globalFrameRow += frameRowStep) {
      unsigned row = globalFrameRow % realDim.y;
      unsigned col = globalFrameRow / realDim.y * slicing.widthOfSlice(0) / cpp;
      if (col >= static_cast<unsigned>(realDim.x))
        break;

      assert(sliceWidth % cpp == 0);
      unsigned pixelsPerSliceRow = sliceWidth / cpp;
      if (col + pixelsPerSliceRow > static_cast<unsigned>(realDim.x))
        ThrowRDE("Bad slice width / frame size / image size combination.");
      if (((sliceId + 1) == slicing.numSlices) &&
          (col + pixelsPerSliceRow != static_cast<unsigned>(realDim.x)))
        ThrowRDE("Insufficient slices - do not fill the entire image");

      row /= Y_S_F;

      assert(col % X_S_F == 0);
      col /= X_S_F;
      col *= colsPerGroup;
      assert(sliceWidth % sliceColStep == 0);
      for (unsigned sliceCol = 0; sliceCol < sliceWidth;) {
        // check if we processed one full raw row worth of pixels
        if (globalFrameCol == (unsigned)frame.x) {
          // if yes -> update predictor by going back exactly one row,
          // no matter where we are right now.
          // makes no sense from an image compression point of view, ask Canon.
          for (int c = 0; c < N_COMP; ++c)
            pred[c] = predNext[c == 0 ? c : groupSize - (N_COMP - c)];
          predNext = &out(row, col);
          globalFrameCol = 0;
        }

        // How many pixel can we decode until we finish the row of either
        // the frame (i.e. predictor change time), or of the current slice?
        assert((unsigned)frame.x % X_S_F == 0);
        unsigned sliceColsRemainingInThisFrameRow =
            sliceColStep * (((unsigned)frame.x - globalFrameCol) / X_S_F);
        unsigned sliceColsRemainingInThisSliceRow = sliceWidth - sliceCol;
        unsigned sliceColsRemaining = std::min(
            sliceColsRemainingInThisSliceRow, sliceColsRemainingInThisFrameRow);
        assert(sliceColsRemaining >= sliceColStep &&
               (sliceColsRemaining % sliceColStep) == 0);
        for (unsigned sliceColEnd = sliceCol + sliceColsRemaining;
             sliceCol < sliceColEnd; sliceCol += sliceColStep,
                      globalFrameCol += X_S_F, col += groupSize) {
          for (int p = 0; p < groupSize; ++p) {
            int c = p < pixelsPerGroup ? 0 : p - pixelsPerGroup + 1;
            out(row, col + p) = pred[c] += ht[c]->decodeDifference(bs);
          }
        }
      }
    }
  }
}

void Cr2Decompressor::decompress() {
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
