/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2017 Axel Waggershauser
    Copyright (C) 2017-2023 Roman Lebedev

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
#include "adt/CroppedArray2DRef.h"        // for CroppedArray2DRef
#include "adt/Invariant.h"                // for invariant
#include "adt/Point.h"                    // for iPoint2D, iRectangle2D
#include "common/Common.h"                // for roundUpDivision
#include "common/RawImage.h"              // for RawImage, RawImageData
#include "decoders/RawDecoderException.h" // for ThrowException, ThrowRDE
#include "io/BitPumpJPEG.h"               // for BitPumpJPEG, BitStream<>::...
#include "io/ByteStream.h"                // for ByteStream
#include <algorithm>                      // for transform
#include <array>                          // for array
#include <cinttypes>                      // for PRId64
#include <memory>                         // for allocator_traits<>::value_...
#include <utility>                        // for move

namespace rawspeed {

LJpegDecompressor::LJpegDecompressor(const RawImage& img,
                                     iRectangle2D imgFrame_, iPoint2D frame_,
                                     iPoint2D MCUSize_,
                                     std::vector<PerComponentRecipe> rec_,
                                     ByteStream bs)
    : mRaw(img), input(bs), imgFrame(imgFrame_), frame(frame_),
      MCUSize(MCUSize_), rec(std::move(rec_)) {
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

  if (imgFrame.pos.x >= mRaw->dim.x)
    ThrowRDE("X offset outside of image");
  if (imgFrame.pos.y >= mRaw->dim.y)
    ThrowRDE("Y offset outside of image");

  if (imgFrame.dim.x > mRaw->dim.x)
    ThrowRDE("Tile wider than image");
  if (imgFrame.dim.y > mRaw->dim.y)
    ThrowRDE("Tile taller than image");

  if (imgFrame.pos.x + imgFrame.dim.x > mRaw->dim.x)
    ThrowRDE("Tile overflows image horizontally");
  if (imgFrame.pos.y + imgFrame.dim.y > mRaw->dim.y)
    ThrowRDE("Tile overflows image vertically");

  auto cps = MCUSize.area();
  if (cps < 1 || cps > 4)
    ThrowRDE("Unsupported number of components: %" PRId64, cps);

  if (MCUSize != iPoint2D(cps, 1) && MCUSize != iPoint2D(2, 2))
    ThrowRDE("Unsupported LJpeg MCU: %i x %i", MCUSize.x, MCUSize.y);

  if (rec.size() != (unsigned)cps)
    ThrowRDE("Must have exactly one recepie per component");

  for (const auto& recip : rec) {
    if (!recip.ht.isFullDecode())
      ThrowRDE("Huffman table is not of a full decoding variety");
  }

  if ((unsigned)cps < mRaw->getCpp())
    ThrowRDE("Unexpected number of components");

  if ((int64_t)cps * frame.x > std::numeric_limits<int>::max())
    ThrowRDE("LJpeg frame is too big");

  if (!(imgFrame.dim >= MCUSize))
    ThrowRDE("Image frame is smaller than a single LJpeg MCU.");

  // How many output pixels are we expected to produce, as per DNG tiling?
  const int tileRequiredWidth = (int)mRaw->getCpp() * imgFrame.dim.x;
  // How many of these rows do we need?
  invariant(imgFrame.dim.y % MCUSize.y == 0);
  const auto numRows = imgFrame.dim.y / MCUSize.y;

  // How many full pixel blocks do we need to consume for that?
  if (const int blocksToConsume = roundUpDivision(tileRequiredWidth, MCUSize.x);
      frame.x < blocksToConsume || frame.y < numRows ||
      (int64_t)MCUSize.x * frame.x < (int64_t)mRaw->getCpp() * imgFrame.dim.x) {
    ThrowRDE("LJpeg frame (%u, %u) is smaller than expected (%u, %u)",
             MCUSize.x * frame.x, frame.y, tileRequiredWidth, numRows);
  }

  // How many full pixel blocks will we produce?
  fullCols = tileRequiredWidth / MCUSize.x; // Truncating division!
  invariant(fullCols > 0);
  // Do we need to also produce part of a block?
  havePartialCol = tileRequiredWidth % MCUSize.x;
}

template <int N_COMP, size_t... I>
std::array<std::reference_wrapper<const PrefixCodeDecoder<>>, N_COMP>
LJpegDecompressor::getPrefixCodeDecodersImpl(
    std::index_sequence<I...> /*unused*/) const {
  return std::array<std::reference_wrapper<const PrefixCodeDecoder<>>, N_COMP>{
      std::cref(rec[I].ht)...};
}

template <int N_COMP>
std::array<std::reference_wrapper<const PrefixCodeDecoder<>>, N_COMP>
LJpegDecompressor::getPrefixCodeDecoders() const {
  return getPrefixCodeDecodersImpl<N_COMP>(std::make_index_sequence<N_COMP>{});
}

template <int N_COMP>
std::array<uint16_t, N_COMP> LJpegDecompressor::getInitialPreds() const {
  std::array<uint16_t, N_COMP> preds;
  std::transform(
      rec.begin(), rec.end(), preds.begin(),
      [](const PerComponentRecipe& compRec) { return compRec.initPred; });
  return preds;
}

// N_COMP == number of components (2, 3 or 4)

namespace {

template <int MCUWidth, int MCUHeight>
constexpr iPoint2D MCU = {MCUWidth, MCUHeight};

} // namespace

template <const iPoint2D& MCU> void LJpegDecompressor::decodeN() {
  invariant(MCU == this->MCUSize);

  invariant(MCU.hasPositiveArea());
  // FIXME: workarounding lack of constexpr std::abs() :(
  constexpr int N_COMP = MCU.x * MCU.y;
  invariant(mRaw->getCpp() > 0);
  invariant(N_COMP > 0);
  invariant(N_COMP >= mRaw->getCpp());
  invariant((N_COMP / mRaw->getCpp()) > 0);

  invariant(mRaw->dim.x >= N_COMP);
  invariant((mRaw->getCpp() * (mRaw->dim.x - imgFrame.pos.x)) >= N_COMP);

  const CroppedArray2DRef img(mRaw->getU16DataAsUncroppedArray2DRef(),
                              mRaw->getCpp() * imgFrame.pos.x, imgFrame.pos.y,
                              mRaw->getCpp() * imgFrame.dim.x, imgFrame.dim.y);

  const auto ht = getPrefixCodeDecoders<N_COMP>();
  auto pred = getInitialPreds<N_COMP>();

  BitPumpJPEG bitStream(input);

  // A recoded DNG might be split up into tiles of self contained LJpeg blobs.
  // The tiles at the bottom and the right may extend beyond the dimension of
  // the raw image buffer. The excessive content has to be ignored.

  // invariant(frame.y >= imgFrame.dim.y); // FIXME
  invariant((int64_t)N_COMP * frame.x >=
            (int64_t)mRaw->getCpp() * imgFrame.dim.x);

  invariant(imgFrame.pos.y + imgFrame.dim.y <= mRaw->dim.y);
  invariant(imgFrame.pos.x + imgFrame.dim.x <= mRaw->dim.x);

  invariant(imgFrame.dim.y % MCU.y == 0);
  const auto numFrameRows = imgFrame.dim.y / MCU.y;

  // For y, we can simply stop decoding when we reached the border.
  invariant(numFrameRows > 0);
  for (int frameRow = 0; frameRow < numFrameRows; ++frameRow) {
    int frameCol = 0;

    // FIXME: predictor may have value outside of the uint16_t.
    // https://github.com/darktable-org/rawspeed/issues/175

    // For x, we first process all full pixel blocks within the image buffer ...
    invariant(fullCols > 0);
    for (; frameCol < fullCols; ++frameCol) {
      for (int MCURow = 0; MCURow != MCU.y; ++MCURow) {
        for (int MCUСol = 0; MCUСol != MCU.x; ++MCUСol) {
          int c = MCU.x * MCURow + MCUСol;
          pred[c] = uint16_t(pred[c] + ((const PrefixCodeDecoder<>&)(ht[c]))
                                           .decodeDifference(bitStream));
          int imgRow = (frameRow * MCU.y) + MCURow;
          int imgCol = (frameCol * MCU.x) + MCUСol;
          img(imgRow, imgCol) = pred[c];
        }
      }
    }

    // Sometimes we also need to consume one more block, and produce part of it.
    if (havePartialCol) {
      invariant(N_COMP > 1 && "can't want part of 1-pixel-wide block");
      // Some rather esoteric DNG's have odd dimensions, e.g. width % 2 = 1.
      // We may end up needing just part of last N_COMP pixels.
      for (int MCURow = 0; MCURow != MCU.y; ++MCURow) {
        for (int MCUСol = 0; MCUСol != MCU.x; ++MCUСol) {
          int c = MCU.x * MCURow + MCUСol;
          pred[c] = uint16_t(pred[c] + ((const PrefixCodeDecoder<>&)(ht[c]))
                                           .decodeDifference(bitStream));
          int imgRow = (frameRow * MCU.y) + MCURow;
          int imgCol = (frameCol * MCU.x) + MCUСol;
          if (imgCol < img.croppedWidth)
            img(imgRow, imgCol) = pred[c];
        }
      }
      ++frameCol; // We did just process one more block.
    }

    // ... and discard the rest.
    for (; frameCol < frame.x; ++frameCol) {
      for (int c = 0; c != N_COMP; ++c)
        ((const PrefixCodeDecoder<>&)(ht[c])).decodeDifference(bitStream);
    }

    // The first sample of the next row is calculated based on the first sample
    // of this row, so copy it for the next iteration
    for (int MCURow = 0; MCURow != MCU.y; ++MCURow) {
      for (int MCUСol = 0; MCUСol != MCU.x; ++MCUСol) {
        pred[MCU.x * MCURow + MCUСol] = img(frameRow * MCU.y + MCURow, MCUСol);
      }
    }
  }
}

void LJpegDecompressor::decode() {
  switch (MCUSize.area()) {
  case 1:
    if (MCUSize == MCU<1, 1>) {
      decodeN<MCU<1, 1>>();
      return;
    }
    break;
  case 2:
    if (MCUSize == MCU<2, 1>) {
      decodeN<MCU<2, 1>>();
      return;
    }
    break;
  case 3:
    if (MCUSize == MCU<3, 1>) {
      decodeN<MCU<3, 1>>();
      return;
    }
    break;
  case 4:
    if (MCUSize == MCU<4, 1>) {
      decodeN<MCU<4, 1>>();
      return;
    }
    if (MCUSize == MCU<2, 2>) {
      decodeN<MCU<2, 2>>();
      return;
    }
    break;
  default:
    __builtin_unreachable();
  }
  __builtin_unreachable();
}

} // namespace rawspeed
