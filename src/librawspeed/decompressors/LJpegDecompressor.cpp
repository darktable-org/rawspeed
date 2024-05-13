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
#include "adt/Array1DRef.h"
#include "adt/Array2DRef.h"
#include "adt/Casts.h"
#include "adt/CroppedArray2DRef.h"
#include "adt/Invariant.h"
#include "adt/Optional.h"
#include "adt/Point.h"
#include "bitstreams/BitStreamerJPEG.h"
#include "codes/PrefixCodeDecoder.h"
#include "common/Common.h"
#include "common/RawImage.h"
#include "decoders/RawDecoderException.h"
#include "decompressors/JpegMarkers.h"
#include "io/Buffer.h"
#include "io/ByteStream.h"
#include "io/Endianness.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <utility>
#include <vector>

using std::copy_n;

namespace rawspeed {

LJpegDecompressor::LJpegDecompressor(RawImage img, iRectangle2D imgFrame_,
                                     Frame frame_,
                                     std::vector<PerComponentRecipe> rec_,
                                     int numLJpegRowsPerRestartInterval_,
                                     Array1DRef<const uint8_t> input_)
    : mRaw(std::move(img)), input(input_), imgFrame(imgFrame_),
      frame(std::move(frame_)), rec(std::move(rec_)),
      numLJpegRowsPerRestartInterval(numLJpegRowsPerRestartInterval_) {

  if (mRaw->getDataType() != RawImageType::UINT16)
    ThrowRDE("Unexpected data type (%u)",
             static_cast<unsigned>(mRaw->getDataType()));

  if ((mRaw->getCpp() != 1 || mRaw->getBpp() != sizeof(uint16_t)) &&
      (mRaw->getCpp() != 2 || mRaw->getBpp() != 2 * sizeof(uint16_t)) &&
      (mRaw->getCpp() != 3 || mRaw->getBpp() != 3 * sizeof(uint16_t)))
    ThrowRDE("Unexpected component count (%u)", mRaw->getCpp());

  if (!mRaw->dim.hasPositiveArea())
    ThrowRDE("Image has zero size");

  if (!imgFrame.hasPositiveArea())
    ThrowRDE("Tile has zero size");

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
  // Yeah, sure, here it would be just dumb to leave this for production :)
  if (mRaw->dim.x > 9728 || mRaw->dim.y > 6656) {
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

  if (!frame.dim.hasPositiveArea())
    ThrowRDE("Frame has zero size");

  if (iPoint2D{1, 1} != frame.mcu && iPoint2D{2, 1} != frame.mcu &&
      iPoint2D{3, 1} != frame.mcu && iPoint2D{4, 1} != frame.mcu &&
      iPoint2D{2, 2} != frame.mcu)
    ThrowRDE("Unexpected MCU size: {%i, %i}", frame.mcu.x, frame.mcu.y);

  if (rec.size() != static_cast<unsigned>(frame.mcu.area()))
    ThrowRDE("Must have exactly one recepie per component");

  for (const auto& recip : rec) {
    if (!recip.ht.isFullDecode())
      ThrowRDE("Huffman table is not of a full decoding variety");
  }

  if (numLJpegRowsPerRestartInterval < 1)
    ThrowRDE("Number of rows per restart interval must be positives");

  if (static_cast<int64_t>(frame.mcu.x) * frame.dim.x >
          std::numeric_limits<int>::max() ||
      static_cast<int64_t>(frame.mcu.y) * frame.dim.y >
          std::numeric_limits<int>::max())
    ThrowRDE("LJpeg frame is too big");

  if (static_cast<int64_t>(mRaw->getCpp()) * imgFrame.dim.x >
      std::numeric_limits<int>::max())
    ThrowRDE("Img frame is too big");

  if (imgFrame.dim.x < frame.mcu.x || imgFrame.dim.y < frame.mcu.y)
    ThrowRDE("Tile size is smaller than a single frame MCU");

  if (imgFrame.dim.y % frame.mcu.y != 0)
    ThrowRDE("Output row count is not a multiple of MCU row count");

  const int tileRequiredWidth =
      static_cast<int>(mRaw->getCpp()) * imgFrame.dim.x;

  // How many full pixel MCUs do we need to consume for that?
  if (const auto mcusToConsume = implicit_cast<int>(
          roundUpDivisionSafe(tileRequiredWidth, frame.mcu.x));
      frame.dim.x < mcusToConsume ||
      frame.mcu.y * frame.dim.y < imgFrame.dim.y ||
      frame.mcu.x * frame.dim.x < tileRequiredWidth) {
    ThrowRDE("LJpeg frame (%d, %d) is smaller than expected (%u, %u)",
             frame.mcu.x * frame.dim.x, frame.mcu.y * frame.dim.y,
             tileRequiredWidth, imgFrame.dim.y);
  }

  // How many full pixel MCUs will we produce?
  numFullMCUs = tileRequiredWidth / frame.mcu.x; // Truncating division!
  // Do we need to also produce part of a MCU?
  trailingPixels = tileRequiredWidth % frame.mcu.x;
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

namespace {

template <int MCUWidth, int MCUHeight>
constexpr iPoint2D MCU = {MCUWidth, MCUHeight};

} // namespace

template <const iPoint2D& MCUSize, int N_COMP>
void LJpegDecompressor::decodeRowN(
    Array2DRef<uint16_t> outStripe, Array2DRef<const uint16_t> pred,
    std::array<std::reference_wrapper<const PrefixCodeDecoder<>>, N_COMP> ht,
    BitStreamerJPEG& bs) const {
  invariant(MCUSize.area() == N_COMP);
  invariant(outStripe.width() >= MCUSize.x);
  invariant(outStripe.height() == MCUSize.y);
  invariant(pred.width() == MCUSize.x);
  invariant(pred.height() == MCUSize.y);

  // FIXME: predictor may have value outside of the uint16_t.
  // https://github.com/darktable-org/rawspeed/issues/175

  int mcuIdx = 0;
  // For x, we first process all full pixel MCUs within the image buffer ...
  for (; mcuIdx < numFullMCUs; ++mcuIdx) {
    const auto outTile = CroppedArray2DRef(outStripe,
                                           /*offsetCols=*/MCUSize.x * mcuIdx,
                                           /*offsetRows=*/0,
                                           /*croppedWidth=*/MCUSize.x,
                                           /*croppedHeight=*/MCUSize.y)
                             .getAsArray2DRef();
    for (int MCURow = 0; MCURow != MCUSize.y; ++MCURow) {
      for (int MCUСol = 0; MCUСol != MCUSize.x; ++MCUСol) {
        int c = MCUSize.x * MCURow + MCUСol;
        int prediction = pred(MCURow, MCUСol);
        int diff = (static_cast<const PrefixCodeDecoder<>&>(ht[c]))
                       .decodeDifference(bs);
        int pix = prediction + diff;
        outTile(MCURow, MCUСol) = uint16_t(pix);
      }
    }
    // The predictor for the next MCU of the row is the just-decoded MCU.
    pred = outTile;
  }

  // Sometimes we also need to consume one more MCU, and produce part of it.
  if (trailingPixels != 0) {
    // Some rather esoteric DNG's have odd dimensions, e.g. width % 2 = 1.
    // We may end up needing just part of last N_COMP pixels.
    invariant(trailingPixels > 0);
    invariant(trailingPixels < N_COMP);
    invariant(N_COMP > 1 && "can't want part of 1-pixel-wide block");
    // Some rather esoteric DNG's have odd dimensions, e.g. width % 2 = 1.
    // We may end up needing just part of last N_COMP pixels.
    for (int MCURow = 0; MCURow != MCUSize.y; ++MCURow) {
      for (int MCUСol = 0; MCUСol != MCUSize.x; ++MCUСol) {
        int c = MCUSize.x * MCURow + MCUСol;
        int prediction = pred(MCURow, MCUСol);
        int diff = (static_cast<const PrefixCodeDecoder<>&>(ht[c]))
                       .decodeDifference(bs);
        int pix = prediction + diff;
        int stripeRow = MCURow;
        int stripeCol = (MCUSize.x * mcuIdx) + MCUСol;
        if (stripeCol < outStripe.width())
          outStripe(stripeRow, stripeCol) = uint16_t(pix);
      }
    }
    ++mcuIdx; // We did just process one more MCU.
  }

  // ... and discard the rest.
  for (; mcuIdx < frame.dim.x; ++mcuIdx) {
    for (int i = 0; i != N_COMP; ++i)
      (static_cast<const PrefixCodeDecoder<>&>(ht[i])).decodeDifference(bs);
  }
}

// N_COMP == number of components (2, 3 or 4)
template <const iPoint2D& MCU>
ByteStream::size_type LJpegDecompressor::decodeN() const {
  invariant(MCU == this->frame.mcu);

  invariant(MCU.hasPositiveArea());
  // FIXME: workarounding lack of constexpr std::abs() :(
  constexpr int N_COMP = MCU.x * MCU.y;

  invariant(mRaw->getCpp() > 0);

  const auto img =
      CroppedArray2DRef(mRaw->getU16DataAsUncroppedArray2DRef(),
                        mRaw->getCpp() * imgFrame.pos.x, imgFrame.pos.y,
                        mRaw->getCpp() * imgFrame.dim.x, imgFrame.dim.y)
          .getAsArray2DRef();

  const auto ht = getPrefixCodeDecoders<N_COMP>();

  // A recoded DNG might be split up into tiles of self contained LJpeg blobs.
  // The tiles at the bottom and the right may extend beyond the dimension of
  // the raw image buffer. The excessive content has to be ignored.

  invariant(imgFrame.dim.y % frame.mcu.y == 0);
  const auto numRestartIntervals = implicit_cast<int>(roundUpDivisionSafe(
      imgFrame.dim.y / frame.mcu.y, numLJpegRowsPerRestartInterval));
  invariant(numRestartIntervals >= 0);
  invariant(numRestartIntervals != 0);

  ByteStream inputStream(DataBuffer(input, Endianness::little));
  for (int restartIntervalIndex = 0;
       restartIntervalIndex != numRestartIntervals; ++restartIntervalIndex) {
    auto predStorage = getInitialPreds<N_COMP>();
    auto pred = Array2DRef(predStorage.data(), MCU.x, MCU.y);

    if (restartIntervalIndex != 0) {
      auto marker = peekMarker(inputStream);
      if (!marker) // FIXME: can there be padding bytes before the marker?
        ThrowRDE("Jpeg marker not encountered");
      Optional<int> number = getRestartMarkerNumber(*marker);
      if (!number)
        ThrowRDE("Not a restart marker!");
      if (*number != ((restartIntervalIndex - 1) % 8))
        ThrowRDE("Unexpected restart marker found");
      inputStream.skipBytes(2); // Good restart marker.
    }

    BitStreamerJPEG bs(inputStream.peekRemainingBuffer().getAsArray1DRef());

    for (int ljpegRowOfRestartInterval = 0;
         ljpegRowOfRestartInterval != numLJpegRowsPerRestartInterval;
         ++ljpegRowOfRestartInterval) {
      const int row =
          frame.mcu.y * (numLJpegRowsPerRestartInterval * restartIntervalIndex +
                         ljpegRowOfRestartInterval);
      invariant(row >= 0);
      invariant(row <= imgFrame.dim.y);

      // For y, we can simply stop decoding when we reached the border.
      if (row == imgFrame.dim.y) {
        invariant((restartIntervalIndex + 1) == numRestartIntervals);
        break;
      }

      const auto outStripe = CroppedArray2DRef(img,
                                               /*offsetCols=*/0,
                                               /*offsetRows=*/row,
                                               /*croppedWidth=*/img.width(),
                                               /*croppedHeight=*/frame.mcu.y)
                                 .getAsArray2DRef();

      decodeRowN<MCU, N_COMP>(outStripe, pred, ht, bs);

      // The predictor for the next line is the start of this line.
      pred = CroppedArray2DRef(outStripe,
                               /*offsetCols=*/0,
                               /*offsetRows=*/0,
                               /*croppedWidth=*/MCU.x,
                               /*croppedHeight=*/MCU.y)
                 .getAsArray2DRef();
    }

    inputStream.skipBytes(bs.getStreamPosition());
  }

  return inputStream.getPosition();
}

ByteStream::size_type LJpegDecompressor::decode() const {
  switch (frame.mcu.area()) {
  case 1:
    if (frame.mcu == MCU<1, 1>) {
      return decodeN<MCU<1, 1>>();
    }
    break;
  case 2:
    if (frame.mcu == MCU<2, 1>) {
      return decodeN<MCU<2, 1>>();
    }
    break;
  case 3:
    if (frame.mcu == MCU<3, 1>) {
      return decodeN<MCU<3, 1>>();
    }
    break;
  case 4:
    if (frame.mcu == MCU<4, 1>) {
      return decodeN<MCU<4, 1>>();
    }
    if (frame.mcu == MCU<2, 2>) {
      return decodeN<MCU<2, 2>>();
    }
    break;
  default:
    __builtin_unreachable();
  }
  __builtin_unreachable();
}

} // namespace rawspeed
