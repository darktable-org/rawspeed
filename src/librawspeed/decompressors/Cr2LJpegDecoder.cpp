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

#include "decompressors/Cr2LJpegDecoder.h"
#include "adt/Casts.h"
#include "adt/Point.h"
#include "codes/PrefixCodeDecoder.h"
#include "common/RawImage.h"
#include "decoders/RawDecoderException.h"
#include "decompressors/AbstractLJpegDecoder.h"
#include "decompressors/Cr2Decompressor.h"
#include "io/Buffer.h"
#include "io/ByteStream.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <iterator>
#include <tuple>
#include <vector>

namespace rawspeed {

Cr2LJpegDecoder::Cr2LJpegDecoder(ByteStream bs, const RawImage& img)
    : AbstractLJpegDecoder(bs, img) {
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

Buffer::size_type Cr2LJpegDecoder::decodeScan() {
  if (numMCUsPerRestartInterval != 0)
    ThrowRDE("Non-zero restart interval not supported.");

  if (predictorMode != 1)
    ThrowRDE("Unsupported predictor mode.");

  if (slicing.empty()) {
    const int slicesWidth = frame.w * frame.cps;
    if (slicesWidth > mRaw->dim.x)
      ThrowRDE("Don't know slicing pattern, and failed to guess it.");

    slicing =
        Cr2SliceWidths(/*numSlices=*/1, /*sliceWidth=don't care*/ 0,
                       /*lastSliceWidth=*/implicit_cast<uint16_t>(slicesWidth));
  }

  bool isSubSampled = false;
  for (uint32_t i = 0; i < frame.cps; i++)
    isSubSampled = isSubSampled || frame.compInfo[i].superH != 1 ||
                   frame.compInfo[i].superV != 1;

  if (frame.cps != 3 && frame.w * frame.cps > 2 * frame.h) {
    // Fix Canon double height issue where Canon doubled the width and halfed
    // the height (e.g. with 5Ds), ask Canon. frame.w needs to stay as is here
    // because the number of pixels after which the predictor gets updated is
    // still the doubled width.
    // see: FIX_CANON_HALF_HEIGHT_DOUBLE_WIDTH
    frame.h *= 2;
  }

  std::tuple<int /*N_COMP*/, int /*X_S_F*/, int /*Y_S_F*/> format;

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
      format = {3, 2, 2}; // Cr2 sRaw1/mRaw
    else {
      assert(frame.compInfo[0].superV == 1);
      // fix the inconsistent slice width in sRaw mode, ask Canon.
      for (auto* width : {&slicing.sliceWidth, &slicing.lastSliceWidth})
        *width = (*width) * 3 / 2;
      format = {3, 2, 1}; // Cr2 sRaw2/sRaw
    }
  } else {
    switch (frame.cps) {
    case 2:
      format = {2, 1, 1};
      break;
    case 4:
      format = {4, 1, 1};
      break;
    default:
      ThrowRDE("Unsupported number of components: %u", frame.cps);
    }
  }

  int N_COMP = std::get<0>(format);

  std::vector<Cr2Decompressor<PrefixCodeDecoder<>>::PerComponentRecipe> rec;
  rec.reserve(N_COMP);
  std::generate_n(
      std::back_inserter(rec), N_COMP,
      [&rec, hts = getPrefixCodeDecoders(N_COMP),
       initPred = getInitialPredictors(N_COMP)]()
          -> Cr2Decompressor<PrefixCodeDecoder<>>::PerComponentRecipe {
        const auto i = implicit_cast<int>(rec.size());
        return {*hts[i], initPred[i]};
      });

  Cr2Decompressor<PrefixCodeDecoder<>> d(
      mRaw, format, iPoint2D(frame.w, frame.h), slicing, rec,
      input.peekRemainingBuffer().getAsArray1DRef());
  return d.decompress();
}

void Cr2LJpegDecoder::decode(const Cr2SliceWidths& slicing_) {
  slicing = slicing_;
  for (auto sliceId = 0; sliceId < slicing.numSlices; sliceId++) {
    const auto sliceWidth = slicing.widthOfSlice(sliceId);
    if (sliceWidth <= 0)
      ThrowRDE("Bad slice width: %i", sliceWidth);
  }

  AbstractLJpegDecoder::decodeSOI();
}

} // namespace rawspeed
