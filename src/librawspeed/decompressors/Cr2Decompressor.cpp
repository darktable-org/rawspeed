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

Cr2Decompressor::Cr2Decompressor(const ByteStream& bs, const RawImage& img)
    : AbstractLJpegDecompressor(bs, img) {
  if (mRaw->getDataType() != RawImageType::UINT16)
    ThrowRDE("Unexpected data type");

  if (mRaw->getCpp() != 1 || mRaw->getBpp() != sizeof(uint16_t))
    ThrowRDE("Unexpected cpp: %u", mRaw->getCpp());

  if (!mRaw->dim.x || !mRaw->dim.y || mRaw->dim.x > 19440 ||
      mRaw->dim.y > 5920) {
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", mRaw->dim.x,
             mRaw->dim.y);
  }
}

void Cr2Decompressor::decodeScan()
{
  if (predictorMode != 1)
    ThrowRDE("Unsupported predictor mode.");

  if (slicing.empty()) {
    const int slicesWidth = frame.w * frame.cps;
    if (slicesWidth > mRaw->dim.x)
      ThrowRDE("Don't know slicing pattern, and failed to guess it.");

    slicing = Cr2Slicing(/*numSlices=*/1, /*sliceWidth=don't care*/ 0,
                         /*lastSliceWidth=*/slicesWidth);
  }

  bool isSubSampled = false;
  for (uint32_t i = 0; i < frame.cps; i++)
    isSubSampled = isSubSampled || frame.compInfo[i].superH != 1 ||
                   frame.compInfo[i].superV != 1;

  if (isSubSampled) {
    if (mRaw->isCFA)
      ThrowRDE("Cannot decode subsampled image to CFA data");

    if (frame.cps != 3)
      ThrowRDE("Unsupported number of subsampled components: %u", frame.cps);

    // see http://lclevy.free.fr/cr2/#sraw for overview table
    bool isSupported = frame.compInfo[0].superH == 2;

    isSupported = isSupported && (frame.compInfo[0].superV == 1 ||
                                  frame.compInfo[0].superV == 2);

    for (uint32_t i = 1; i < frame.cps; i++)
      isSupported = isSupported && frame.compInfo[i].superH == 1 &&
                    frame.compInfo[i].superV == 1;

    if (!isSupported) {
      ThrowRDE("Unsupported subsampling ([[%u, %u], [%u, %u], [%u, %u]])",
               frame.compInfo[0].superH, frame.compInfo[0].superV,
               frame.compInfo[1].superH, frame.compInfo[1].superV,
               frame.compInfo[2].superH, frame.compInfo[2].superV);
    }

    if (frame.compInfo[0].superV == 2)
      decodeN_X_Y<3, 2, 2>(); // Cr2 sRaw1/mRaw
    else {
      assert(frame.compInfo[0].superV == 1);
      // fix the inconsistent slice width in sRaw mode, ask Canon.
      for (auto* width : {&slicing.sliceWidth, &slicing.lastSliceWidth})
        *width = (*width) * 3 / 2;
      decodeN_X_Y<3, 2, 1>(); // Cr2 sRaw2/sRaw
    }
  } else {
    switch (frame.cps) {
    case 2:
      decodeN_X_Y<2, 1, 1>();
      break;
    case 4:
      decodeN_X_Y<4, 1, 1>();
      break;
    default:
      ThrowRDE("Unsupported number of components: %u", frame.cps);
    }
  }
}

void Cr2Decompressor::decode(const Cr2Slicing& slicing_) {
  slicing = slicing_;
  for (auto sliceId = 0; sliceId < slicing.numSlices; sliceId++) {
    const auto sliceWidth = slicing.widthOfSlice(sliceId);
    if (sliceWidth <= 0)
      ThrowRDE("Bad slice width: %i", sliceWidth);
  }

  AbstractLJpegDecompressor::decode();
}

// N_COMP == number of components (2, 3 or 4)
// X_S_F  == x/horizontal sampling factor (1 or 2)
// Y_S_F  == y/vertical   sampling factor (1 or 2)

template <int N_COMP, int X_S_F, int Y_S_F>
void Cr2Decompressor::decodeN_X_Y()
{
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

  auto ht = getHuffmanTables<N_COMP>();
  auto pred = getInitialPredictors<N_COMP>();
  const auto* predNext = &out(0, 0);

  BitPumpJPEG bs(input);

  if (frame.cps != 3 && frame.w * frame.cps > 2 * frame.h) {
    // Fix Canon double height issue where Canon doubled the width and halfed
    // the height (e.g. with 5Ds), ask Canon. frame.w needs to stay as is here
    // because the number of pixels after which the predictor gets updated is
    // still the doubled width.
    // see: FIX_CANON_HALF_HEIGHT_DOUBLE_WIDTH
    frame.h *= 2;
  }

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

  if (iPoint2D::area_type(frame.h) * slicing.totalWidth() <
      cpp * realDim.area())
    ThrowRDE("Incorrect slice height / slice widths! Less than image size.");

  unsigned globalFrameCol = 0;
  unsigned globalFrameRow = 0;
  for (auto sliceId = 0; sliceId < slicing.numSlices; sliceId++) {
    const unsigned sliceWidth = slicing.widthOfSlice(sliceId);

    assert(frame.h % frameRowStep == 0);
    for (unsigned sliceFrameRow = 0; sliceFrameRow < frame.h;
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
        if (globalFrameCol == frame.w) {
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
        assert(frame.w % X_S_F == 0);
        unsigned sliceColsRemainingInThisFrameRow =
            sliceColStep * ((frame.w - globalFrameCol) / X_S_F);
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

} // namespace rawspeed
