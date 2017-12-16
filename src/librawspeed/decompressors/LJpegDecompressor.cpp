/*
    RawSpeed - RAW file decoder.

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

#include "decompressors/LJpegDecompressor.h"
#include "common/Common.h"                // for uint32, unroll_loop, ushort16
#include "common/Point.h"                 // for iPoint2D
#include "common/RawImage.h"              // for RawImage, RawImageData
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "io/BitPumpJPEG.h"               // for BitPumpJPEG
#include <algorithm>                      // for min, copy_n

using std::copy_n;

namespace rawspeed {

LJpegDecompressor::LJpegDecompressor(const ByteStream& bs, const RawImage& img)
    : AbstractLJpegDecompressor(bs, img) {
  if (mRaw->getDataType() != TYPE_USHORT16)
    ThrowRDE("Unexpected data type (%u)", mRaw->getDataType());

  if (!((mRaw->getCpp() == 1 && mRaw->getBpp() == 2) ||
        (mRaw->getCpp() == 3 && mRaw->getBpp() == 6)))
    ThrowRDE("Unexpected component count (%u)", mRaw->getCpp());

  if (mRaw->dim.x == 0 || mRaw->dim.y == 0)
    ThrowRDE("Image has zero size");

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
  // Yeah, sure, here it would be just dumb to leave this for production :)
  if (mRaw->dim.x > 7424 || mRaw->dim.y > 5552) {
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", mRaw->dim.x,
             mRaw->dim.y);
  }
#endif
}

void LJpegDecompressor::decode(uint32 offsetX, uint32 offsetY, uint32 width,
                               uint32 height, bool fixDng16Bug_) {
  if (offsetX >= static_cast<unsigned>(mRaw->dim.x))
    ThrowRDE("X offset outside of image");
  if (offsetY >= static_cast<unsigned>(mRaw->dim.y))
    ThrowRDE("Y offset outside of image");

  offX = offsetX;
  offY = offsetY;
  w = width;
  h = height;

  fixDng16Bug = fixDng16Bug_;

  AbstractLJpegDecompressor::decode();
}

void LJpegDecompressor::decodeScan()
{
  if (predictorMode != 1)
    ThrowRDE("Unsupported predictor mode: %u", predictorMode);

  for (uint32 i = 0; i < frame.cps;  i++)
    if (frame.compInfo[i].superH != 1 || frame.compInfo[i].superV != 1)
      ThrowRDE("Unsupported subsampling");

  assert(static_cast<unsigned>(mRaw->dim.x) > offX);
  if ((mRaw->getCpp() * (mRaw->dim.x - offX)) < frame.cps)
    ThrowRDE("Got less pixels than the components per sample");

  switch (frame.cps) {
  case 2:
    decodeN<2>();
    break;
  case 3:
    decodeN<3>();
    break;
  case 4:
    decodeN<4>();
    break;
  default:
    ThrowRDE("Unsupported number of components: %u", frame.cps);
  }
}

// N_COMP == number of components (2, 3 or 4)

template <int N_COMP>
void LJpegDecompressor::decodeN()
{
  assert(mRaw->getCpp() > 0);
  assert(N_COMP > 0);
  assert(N_COMP >= mRaw->getCpp());
  assert((N_COMP / mRaw->getCpp()) > 0);

  assert(mRaw->dim.x >= N_COMP);
  assert((mRaw->getCpp() * (mRaw->dim.x - offX)) >= N_COMP);

  auto ht = getHuffmanTables<N_COMP>();
  auto pred = getInitialPredictors<N_COMP>();
  auto predNext = pred.data();

  BitPumpJPEG bitStream(input);

  for (unsigned y = 0; y < frame.h; ++y) {
    auto destY = offY + y;
    // A recoded DNG might be split up into tiles of self contained LJpeg
    // blobs. The tiles at the bottom and the right may extend beyond the
    // dimension of the raw image buffer. The excessive content has to be
    // ignored. For y, we can simply stop decoding when we reached the border.
    if (destY >= static_cast<unsigned>(mRaw->dim.y))
      break;

    auto dest =
        reinterpret_cast<ushort16*>(mRaw->getDataUncropped(offX, destY));

    copy_n(predNext, N_COMP, pred.data());
    // the predictor for the next line is the start of this line
    predNext = dest;

    unsigned width =
        std::min(frame.w, (mRaw->getCpp() * (mRaw->dim.x - offX)) / N_COMP);

    // For x, we first process all pixels within the image buffer ...
    for (unsigned x = 0; x < width; ++x) {
      unroll_loop<N_COMP>([&](int i) {
        *dest++ = pred[i] += ht[i]->decodeNext(bitStream);
      });
    }
    // ... and discard the rest.
    for (unsigned x = width; x < frame.w; ++x) {
      unroll_loop<N_COMP>([&](int i) {
        ht[i]->decodeNext(bitStream);
      });
    }
  }
}

} // namespace rawspeed
