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

namespace RawSpeed {

void LJpegPlain::decodeScan() {

  if (pred != 1)
    ThrowRDE("LJpegDecompressor::decodeScan: Unsupported prediction direction.");

  // Fix for Canon 6D mRaw, which has flipped width & height for some part of the image
  // We temporarily swap width and height for cropping.
  if (mCanonFlipDim) {
    uint32 w = frame.w;
    frame.w = frame.h;
    frame.h = w;
  }

  // If image attempts to decode beyond the image bounds, strip it.
  if ((frame.w * frame.cps + offX * mRaw->getCpp()) > mRaw->dim.x * mRaw->getCpp())
    skipX = ((frame.w * frame.cps + offX * mRaw->getCpp()) - mRaw->dim.x * mRaw->getCpp()) / frame.cps;
  if (frame.h + offY > (uint32)mRaw->dim.y)
    skipY = frame.h + offY - mRaw->dim.y;

  // Swap back (see above)
  if (mCanonFlipDim) {
    uint32 w = frame.w;
    frame.w = frame.h;
    frame.h = w;
  }

  if (frame.h == 0 || frame.w == 0)
    ThrowRDE("LJpegPlain::decodeScan: Image width or height set to zero");

  /* Correct wrong slice count (Canon G16) */
  if (slicesW.size() == 1)
    slicesW[0] = frame.w * frame.cps;

  if (slicesW.empty())
    slicesW.push_back(frame.w*frame.cps);

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

    if (frame.compInfo[0].superV == 2) {
      // Something like Cr2 sRaw1, use fast decoder
      decodeN_X_Y<3, 2, 2>();
    } else { // frame.compInfo[0].superV == 1
      if (mCanonFlipDim)
        ThrowRDE("LJpegDecompressor::decodeScan: Cannot flip non 4:2:2 subsampled images.");
      // Something like Cr2 sRaw2, use fast decoder
      decodeN_X_Y<3, 2, 1>();
    }
  } else {
    if (mCanonFlipDim)
      ThrowRDE("LJpegDecompressor::decodeScan: Cannot flip non subsampled images.");

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
  _ASSERTE(slicesW.size() < 16);  // We only have 4 bits for slice number.
  _ASSERTE(!(slicesW.size() > 1 && skipX));
  _ASSERTE(frame.compInfo[0].superH == X_S_F);
  _ASSERTE(frame.compInfo[0].superV == Y_S_F);
  _ASSERTE(frame.compInfo[1].superH == 1);
  _ASSERTE(frame.compInfo[1].superV == 1);
  _ASSERTE(frame.cps == N_COMP);
  _ASSERTE(skipX == 0 || X_S_F == 1);

  // old code said this is only relevant for full-res raws
  mCanonDoubleHeight &= Y_S_F == 1 && X_S_F == 1;
  // old code said this is only relevant for s/mRaws
  mCanonFlipDim &= X_S_F == 2;

  if (mCanonDoubleHeight) {
    mRaw->destroyData();
    frame.h *= 2;
    mRaw->dim = iPoint2D(frame.w * 2, frame.h);
    mRaw->createData();
  }

  HuffmanTable *ht[N_COMP] = {nullptr};
  for (int i = 0; i < N_COMP; ++i)
    ht[i] = huff[frame.compInfo[i].dcTblNo];

  mRaw->metadata.subsampling.x = X_S_F;
  mRaw->metadata.subsampling.y = Y_S_F;

  // Fix for Canon 6D mRaw, which has flipped width & height
  uint32 real_h = mCanonFlipDim ? frame.w : frame.h;

  // Initialize predictors
  int p[N_COMP];
  for (int i = 0; i < N_COMP; ++i)
    p[i] = (1 << (frame.prec - Pt - 1));

  uint32 cw = (frame.w - skipX);
  uint32 ch = (frame.h - skipY);

  if (mCanonDoubleHeight)
    ch = frame.h / 2;

  // Fix for Canon 80D mraw format.
  // In that format, `frame` is 4032x3402, while `mRaw` is 4536x3024.
  // Consequently, the slices in `frame` wrap around (this is taken care of by
  // `offset`) and must be decoded fully (without skipY) to fill the image
  if (mWrappedCr2Slices)
    ch = frame.h;

  BitPumpJPEG bitStream(*input);
  uint32 pixel_pitch = mRaw->pitch / 2; // Pitch in pixel

  // To understand the CR2 slice handling and sampling factor behavior, see
  // https://github.com/lclevy/libcraw2/blob/master/docs/cr2_lossless.pdf?raw=true

  uint32 t_s = 0, t_x = 0, t_y = 0;
  constexpr int div = X_S_F == 2 ? Y_S_F+1 : N_COMP;
  // in full raw (<N,1,1>) the width of a slice is the number of raw pixels,
  // i.e. frame.w * frame.cps
  uint32 pixInSlicedLine = 0;
  auto *dest = (ushort16 *)mRaw->getDataUncropped(offX, offY);

  for (uint32 y = 0; y < ch; y += Y_S_F) {
    const ushort16* predict = dest;
    for (uint32 x = 0; x < cw; x += X_S_F) {
      // inner loop decodes one group of pixels at a time
      //  * for <N,1,1>: N  = N*1*1 (full raw)
      //  * for <3,2,1>: 6  = 3*2*1
      //  * for <3,2,2>: 12 = 3*2*2

      if (pixInSlicedLine == 0) { // Next slice
        pixInSlicedLine = slicesW[t_s] / div;
        dest = (ushort16*)mRaw->getDataUncropped(t_x + offX, t_y + offY);
        if (x == 0 && y != 0)
          predict = dest;

        t_y += Y_S_F;
        if (t_y >= (real_h - skipY)) {
          t_y = 0;
          if (X_S_F == 2)
            t_x += slicesW[t_s] / div;
          else
            t_x += slicesW[t_s];
          ++t_s;
        }
      }

      if (X_S_F == 1) { // will be optimized out
        unroll_loop<N_COMP>([&](int i) {
          *dest++ = p[i] += ht[i]->decodeNext(bitStream);
        });
      } else {
        unroll_loop<Y_S_F>([&](int i) {
          dest[0 + i*pixel_pitch] = p[0] += ht[0]->decodeNext(bitStream);
          dest[3 + i*pixel_pitch] = p[0] += ht[0]->decodeNext(bitStream);
        });

        dest[1] = p[1] += ht[1]->decodeNext(bitStream);
        dest[2] = p[2] += ht[2]->decodeNext(bitStream);

        dest += 3 * sizeof(ushort16);
      }

      pixInSlicedLine -= X_S_F;
    }

    if (X_S_F == 1) // will be optimized out
      for (uint32 i = 0; i < skipX; i++)
        unroll_loop<N_COMP>([&](int j) { ht[j]->decodeNext(bitStream); });

    // Update predictors
    unroll_loop<N_COMP>([&](int i) {
      p[i] = predict[i];
    });
  }

  input->skipBytes(bitStream.getBufferPosition());
}

} // namespace RawSpeed
