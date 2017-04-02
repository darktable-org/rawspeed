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

#include "decompressors/Cr2Decompressor.h"
#include "common/Common.h"                // for unroll_loop, uint32, ushort16
#include "common/Point.h"                 // for iPoint2D
#include "common/RawImage.h"              // for RawImage, RawImageData
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "io/BitPumpJPEG.h"               // for BitStream<>::getBufferPosi...
#include "io/ByteStream.h"                // for ByteStream
#include <algorithm>                      // for min, copy_n, move
#include <cassert>                        // for assert

using namespace std;

namespace RawSpeed {

void Cr2Decompressor::decodeScan()
{
  if (predictorMode != 1)
    ThrowRDE("Unsupported predictor mode.");

  if (slicesWidths.empty())
    slicesWidths.push_back(frame.w * frame.cps);

  bool isSubSampled = false;
  for (uint32 i = 0; i < frame.cps;  i++)
    isSubSampled = isSubSampled || frame.compInfo[i].superH != 1 ||
                   frame.compInfo[i].superV != 1;

  if (isSubSampled) {
    if (mRaw->isCFA)
      ThrowRDE("Cannot decode subsampled image to CFA data");

    if (mRaw->getCpp() != frame.cps)
      ThrowRDE("Subsampled component count does not match image.");

    if (frame.cps != 3 || frame.compInfo[0].superH != 2 ||
        (frame.compInfo[0].superV != 2 && frame.compInfo[0].superV != 1) ||
        frame.compInfo[1].superH != 1 || frame.compInfo[1].superV != 1 ||
        frame.compInfo[2].superH != 1 || frame.compInfo[2].superV != 1)
      ThrowRDE("Unsupported subsampling");

    if (frame.compInfo[0].superV == 2)
      decodeN_X_Y<3, 2, 2>(); // Cr2 sRaw1/mRaw
    else {
      assert(frame.compInfo[0].superV == 1);
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

void Cr2Decompressor::decode(std::vector<int> slicesWidths_)
{
  slicesWidths = move(slicesWidths_);
  AbstractLJpegDecompressor::decode();
}

// N_COMP == number of components (2, 3 or 4)
// X_S_F  == x/horizontal sampling factor (1 or 2)
// Y_S_F  == y/vertical   sampling factor (1 or 2)

template <int N_COMP, int X_S_F, int Y_S_F>
void Cr2Decompressor::decodeN_X_Y()
{
  auto ht = getHuffmanTables<N_COMP>();
  auto pred = getInitialPredictors<N_COMP>();
  auto predNext = (ushort16*)mRaw->getDataUncropped(0, 0);

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

  if (X_S_F == 2 && Y_S_F == 1)
  {
    // fix the inconsistent slice width in sRaw mode, ask Canon.
    for (auto& sliceWidth : slicesWidths)
      sliceWidth = sliceWidth * 3 / 2;
  }

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
  for (unsigned sliceWidth : slicesWidths) {
    for (unsigned y = 0; y < frame.h; y += yStepSize) {
      // Fix for Canon 80D mraw format.
      // In that format, `frame` is 4032x3402, while `mRaw` is 4536x3024.
      // Consequently, the slices in `frame` wrap around plus there are few
      // 'extra' sliced lines because sum(slicesW) * sliceH > mRaw->dim.area()
      // Those would overflow, hence the break.
      // see FIX_CANON_FRAME_VS_IMAGE_SIZE_MISMATCH
      unsigned destY = processedLineSlices % mRaw->dim.y;
      unsigned destX =
          processedLineSlices / mRaw->dim.y * slicesWidths[0] / mRaw->getCpp();
      if (destX >= (unsigned)mRaw->dim.x)
        break;
      auto dest = (ushort16*)mRaw->getDataUncropped(destX, destY);

      for (unsigned x = 0; x < sliceWidth; x += xStepSize) {
        // check if we processed one full raw row worth of pixels
        if (processedPixels == frame.w) {
          // if yes -> update predictor by going back exactly one row,
          // no matter where we are right now.
          // makes no sense from an image compression point of view, ask Canon.
          copy_n(predNext, N_COMP, pred.data());
          predNext = dest;
          processedPixels = 0;
        }

        if (X_S_F == 1) { // will be optimized out
          unroll_loop<N_COMP>([&](int i) {
            *dest++ = pred[i] += ht[i]->decodeNext(bitStream);
          });
        } else {
          unroll_loop<Y_S_F>([&](int i) {
            dest[0 + i*pixelPitch] = pred[0] += ht[0]->decodeNext(bitStream);
            dest[3 + i*pixelPitch] = pred[0] += ht[0]->decodeNext(bitStream);
          });

          dest[1] = pred[1] += ht[1]->decodeNext(bitStream);
          dest[2] = pred[2] += ht[2]->decodeNext(bitStream);

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
