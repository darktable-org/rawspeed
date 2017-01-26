/*
RawSpeed - RAW file decoder.

Copyright (C) 2009-2014 Klaus Post
Copyright (C) 2017 Axel Waggershauser

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

#include "decompressors/LJpegPlain.h"
#include "common/Common.h"
#include "common/Point.h"
#include "io/BitPumpJPEG.h"
#include "io/ByteStream.h"
#include "tiff/TiffTag.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cfloat>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

namespace RawSpeed {

void LJpegPlain::decodeScan() {

  if (pred != 1)
    ThrowRDE("LJpegDecompressor::decodeScan: Unsupported prediction direction.");

  if (frame.h == 0 || frame.w == 0)
    ThrowRDE("LJpegPlain::decodeScan: Image width or height set to zero");

  if (slicesW.empty())
    slicesW.push_back(frame.w * frame.cps);

  bool isSubSampled = false;
  for (uint32 i = 0; i < frame.cps;  i++)
    isSubSampled |= frame.compInfo[i].superH != 1 || frame.compInfo[i].superV != 1;

  if (isSubSampled) {
    if (mRaw->isCFA)
      ThrowRDE("LJpegDecompressor::decodeScan: Cannot decode subsampled image to CFA data");

    if (mRaw->getCpp() != frame.cps)
      ThrowRDE("LJpegDecompressor::decodeScan: Subsampled component count does not match image.");

    if (frame.cps != 3 || frame.compInfo[0].superH != 2 ||
        (frame.compInfo[0].superV != 2 && frame.compInfo[0].superV != 1) ||
        frame.compInfo[1].superH != 1 || frame.compInfo[1].superV != 1 ||
        frame.compInfo[2].superH != 1 || frame.compInfo[2].superV != 1)
      ThrowRDE("LJpegDecompressor::decodeScan: Unsupported subsampling");

    if (frame.compInfo[0].superV == 2)
      // Something like Cr2 sRaw1, use fast decoder
      decodeN_X_Y<3, 2, 2>();
    else // frame.compInfo[0].superV == 1
      // Something like Cr2 sRaw2, use fast decoder
      decodeN_X_Y<3, 2, 1>();
  } else {
    if (frame.cps == 2)
      decodeN_X_Y<2, 1, 1>();
    else if (frame.cps == 4)
      decodeN_X_Y<4, 1, 1>();
    else
      ThrowRDE("LJpegDecompressor::decodeScan: Unsupported component direction count.");
  }
}

// little 'forced' loop unrolling helper tool, example:
//   unroll_loop<N>([&](int i) {
//     func(i);
//   });
// will translate to:
//   func(0); func(1); func(2); ... func(N-1);

template <typename Lambda, size_t N>
struct unroll_loop_t {
  inline static void repeat(const Lambda& f) {
    unroll_loop_t<Lambda, N-1>::repeat(f);
    f(N-1);
  }
};

template <typename Lambda>
struct unroll_loop_t<Lambda, 0> {
  inline static void repeat(const Lambda& f) {}
};

template <size_t N, typename Lambda>
inline void unroll_loop(const Lambda& f) {
  unroll_loop_t<Lambda, N>::repeat(f);
}

// N_COMP == number of components (2, 3 or 4)
// X_S_F  == x/horizontal sampling factor (1 or 2)
// Y_S_F  == y/vertical   sampling factor (1 or 2)

template<int N_COMP, int X_S_F, int Y_S_F>
void LJpegPlain::decodeN_X_Y() {
  _ASSERTE(frame.compInfo[0].superH == X_S_F);
  _ASSERTE(frame.compInfo[0].superV == Y_S_F);
  _ASSERTE(frame.compInfo[1].superH == 1);
  _ASSERTE(frame.compInfo[1].superV == 1);
  _ASSERTE(frame.cps == N_COMP);

  HuffmanTable *ht[N_COMP] = {nullptr};
  for (int i = 0; i < N_COMP; ++i)
    ht[i] = huff[frame.compInfo[i].dcTblNo];

  // Initialize predictors
  int p[N_COMP];
  for (int i = 0; i < N_COMP; ++i)
    p[i] = (1 << (frame.prec - Pt - 1));

  BitPumpJPEG bitStream(input);
  uint32 pixelPitch = mRaw->pitch / 2; // Pitch in pixel
  if (frame.cps != 3 && frame.w * frame.cps > 2 * frame.h) {
    // Fix Canon double height issue where Canon doubled the width and halfed
    // the height (e.g. with 5Ds), ask Canon. frame.w needs to stay as is here
    // because the number of pixels after which the predictor gets updated is
    // still the doubled width.
    // see: FIX_CANON_HALF_HEIGHT_DOUBLE_WIDTH
    frame.h *= 2;
  }
  // Fix for Canon 6D mRaw, which has flipped width & height
  // see FIX_CANON_FLIPPED_WIDTH_AND_HEIGHT
  uint32 sliceH = frame.cps == 3 ? min(frame.w, frame.h) : frame.h;

  if (X_S_F == 2 && Y_S_F == 1)
    // fix the inconsistent slice width in sRaw mode, ask Canon.
    for (auto& sliceW : slicesW)
      sliceW = sliceW * 3 / 2;

  // To understand the CR2 slice handling and sampling factor behavior, see
  // https://github.com/lclevy/libcraw2/blob/master/docs/cr2_lossless.pdf?raw=true

  // inner loop decodes one group of pixels at a time
  //  * for <N,1,1>: N  = N*1*1 (full raw)
  //  * for <3,2,1>: 6  = 3*2*1
  //  * for <3,2,2>: 12 = 3*2*2
  // and advances x by N_COMP*X_S_F and y by Y_S_F
  constexpr int xStepSize = N_COMP * X_S_F;
  constexpr int yStepSize = Y_S_F;

  unsigned processedPixels = 0;
  unsigned processedLineSlices = 0;
  auto nextPredictor = (ushort16*)mRaw->getDataUncropped(offX/mRaw->getCpp(), offY);
  for (unsigned sliceW : slicesW) {
    for (unsigned y = 0; y < sliceH; y += yStepSize) {
      // Fix for Canon 80D mraw format.
      // In that format, `frame` is 4032x3402, while `mRaw` is 4536x3024.
      // Consequently, the slices in `frame` wrap around plus there are few
      // 'extra' sliced lines because sum(slicesW) * sliceH > mRaw->dim.area()
      // Those would overflow, hence the break.
      // see FIX_CANON_FRAME_VS_IMAGE_SIZE_MISMATCH
      unsigned destX = processedLineSlices / mRaw->dim.y * slicesW[0];
      unsigned destY = processedLineSlices % mRaw->dim.y;
      if (destX + offX >= mRaw->dim.x * mRaw->getCpp())
        break;
      auto dest = (ushort16*)mRaw->getDataUncropped((destX + offX)/mRaw->getCpp(), destY + offY);
      for (unsigned x = 0; x < sliceW; x += xStepSize) {

        // check if we processed one full raw row worth of pixels
        if (processedPixels == frame.w) {
          // if yes -> update predictor by going back exactly one row,
          // no matter where we are right now.
          // makes no sense from an image compression point of view, ask Canon.
          unroll_loop<N_COMP>([&](int i) {
            p[i] = nextPredictor[i];
          });
          nextPredictor = dest;
          processedPixels = 0;
        }

        if (X_S_F == 1) { // will be optimized out
          unroll_loop<N_COMP>([&](int i) {
            *dest++ = p[i] += ht[i]->decodeNext(bitStream);
          });
        } else {
          unroll_loop<Y_S_F>([&](int i) {
            dest[0 + i*pixelPitch] = p[0] += ht[0]->decodeNext(bitStream);
            dest[3 + i*pixelPitch] = p[0] += ht[0]->decodeNext(bitStream);
          });

          dest[1] = p[1] += ht[1]->decodeNext(bitStream);
          dest[2] = p[2] += ht[2]->decodeNext(bitStream);

          dest += xStepSize;
        }

        processedPixels += X_S_F;
      }
      processedLineSlices += yStepSize;
    }
  }
  input.skipBytes(bitStream.getBufferPosition());
}

} // namespace RawSpeed
