/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2017 Axel Waggershauser
    Copyright (C) 2017 Roman Lebedev

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

#include "decompressors/LJpegDecoder.h"
#include "adt/Invariant.h"                   // for invariant
#include "adt/Point.h"                       // for iPoint2D, iRectangle2D
#include "codes/PrefixCodeDecoder.h"         // for PrefixCodeDecoder
#include "common/RawImage.h"                 // for RawImage, RawImageData
#include "decoders/RawDecoderException.h"    // for ThrowException, ThrowRDE
#include "decompressors/LJpegDecompressor.h" // for LJpegDecompressor::PerC...
#include "io/ByteStream.h"                   // for ByteStream
#include <algorithm>                         // for generate_n
#include <array>                             // for array
#include <iterator>                          // for back_insert_iterator
#include <vector>                            // for vector

using std::copy_n;

namespace rawspeed {

LJpegDecoder::LJpegDecoder(ByteStream bs, const RawImage& img,
                           bool interleaveRows_)
    : AbstractLJpegDecoder(bs, img), interleaveRows{interleaveRows_} {
  if (mRaw->getDataType() != RawImageType::UINT16)
    ThrowRDE("Unexpected data type (%u)",
             static_cast<unsigned>(mRaw->getDataType()));

  if ((mRaw->getCpp() != 1 || mRaw->getBpp() != sizeof(uint16_t)) &&
      (mRaw->getCpp() != 2 || mRaw->getBpp() != 2 * sizeof(uint16_t)) &&
      (mRaw->getCpp() != 3 || mRaw->getBpp() != 3 * sizeof(uint16_t)))
    ThrowRDE("Unexpected component count (%u)", mRaw->getCpp());

  if (!mRaw->dim.hasPositiveArea())
    ThrowRDE("Image has zero size");

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
  // Yeah, sure, here it would be just dumb to leave this for production :)
  if (mRaw->dim.x > 7424 || mRaw->dim.y > 5552) {
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", mRaw->dim.x,
             mRaw->dim.y);
  }
#endif
}

void LJpegDecoder::decode(uint32_t offsetX, uint32_t offsetY, uint32_t width,
                          uint32_t height, bool fixDng16Bug_) {
  if (offsetX >= static_cast<unsigned>(mRaw->dim.x))
    ThrowRDE("X offset outside of image");
  if (offsetY >= static_cast<unsigned>(mRaw->dim.y))
    ThrowRDE("Y offset outside of image");

  if (width > static_cast<unsigned>(mRaw->dim.x))
    ThrowRDE("Tile wider than image");
  if (height > static_cast<unsigned>(mRaw->dim.y))
    ThrowRDE("Tile taller than image");

  if (offsetX + width > static_cast<unsigned>(mRaw->dim.x))
    ThrowRDE("Tile overflows image horizontally");
  if (offsetY + height > static_cast<unsigned>(mRaw->dim.y))
    ThrowRDE("Tile overflows image vertically");

  if (width == 0 || height == 0)
    return; // We do not need anything from this tile.

  offX = offsetX;
  offY = offsetY;
  w = width;
  h = height;

  fixDng16Bug = fixDng16Bug_;

  AbstractLJpegDecoder::decodeSOI();
}

void LJpegDecoder::decodeScan() {
  invariant(frame.cps > 0);

  if (predictorMode != 1)
    ThrowRDE("Unsupported predictor mode: %u", predictorMode);

  for (uint32_t i = 0; i < frame.cps; i++)
    if (frame.compInfo[i].superH != 1 || frame.compInfo[i].superV != 1)
      ThrowRDE("Unsupported subsampling");

  int N_COMP = frame.cps;

  const iPoint2D MCUSize = !interleaveRows
                               ? iPoint2D(frame.cps, 1)
                               : iPoint2D(frame.cps / 2, frame.cps / 2);

  std::vector<LJpegDecompressor::PerComponentRecipe> rec;
  rec.reserve(N_COMP);
  std::generate_n(std::back_inserter(rec), N_COMP,
                  [&rec, hts = getPrefixCodeDecoders(N_COMP),
                   initPred = getInitialPredictors(
                       N_COMP)]() -> LJpegDecompressor::PerComponentRecipe {
                    const int i = rec.size();
                    return {*hts[i], initPred[i]};
                  });

  LJpegDecompressor d(
      mRaw, iRectangle2D({(int)offX, (int)offY}, {(int)w, (int)h}),
      LJpegDecompressor::Frame{N_COMP, iPoint2D(frame.w, frame.h)}, MCUSize,
      rec, input);
  d.decode();
}

} // namespace rawspeed
