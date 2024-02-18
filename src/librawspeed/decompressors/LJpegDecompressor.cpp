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
#include <cinttypes>
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
                                     int numRowsPerRestartInterval_,
                                     Array1DRef<const uint8_t> input_)
    : mRaw(std::move(img)), input(input_), imgFrame(imgFrame_),
      frame(std::move(frame_)), rec(std::move(rec_)),
      numRowsPerRestartInterval(numRowsPerRestartInterval_) {

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

  if (frame.cps < 1 || frame.cps > 4)
    ThrowRDE("Unsupported number of components: %u", frame.cps);

  if (rec.size() != static_cast<unsigned>(frame.cps))
    ThrowRDE("Must have exactly one recepie per component");

  for (const auto& recip : rec) {
    if (!recip.ht.isFullDecode())
      ThrowRDE("Huffman table is not of a full decoding variety");
  }

  // We assume that the tile width requires at least one frame column.
  if (imgFrame.dim.x < frame.cps)
    ThrowRDE("Tile width is smaller than the frame cps");

  if (static_cast<int64_t>(frame.cps) * frame.dim.x >
      std::numeric_limits<int>::max())
    ThrowRDE("LJpeg frame is too big");

  invariant(mRaw->dim.x > imgFrame.pos.x);
  if ((static_cast<int>(mRaw->getCpp()) * (mRaw->dim.x - imgFrame.pos.x)) <
      frame.cps)
    ThrowRDE("Got less pixels than the components per sample");

  // How many output pixels are we expected to produce, as per DNG tiling?
  const int tileRequiredWidth =
      static_cast<int>(mRaw->getCpp()) * imgFrame.dim.x;

  // How many full pixel blocks do we need to consume for that?
  if (const auto blocksToConsume =
          implicit_cast<int>(roundUpDivision(tileRequiredWidth, frame.cps));
      frame.dim.x < blocksToConsume || frame.dim.y < imgFrame.dim.y ||
      static_cast<int64_t>(frame.cps) * frame.dim.x <
          static_cast<int64_t>(mRaw->getCpp()) * imgFrame.dim.x) {
    ThrowRDE("LJpeg frame (%" PRIu64 ", %u) is smaller than expected (%u, %u)",
             static_cast<int64_t>(frame.cps) * frame.dim.x, frame.dim.y,
             tileRequiredWidth, imgFrame.dim.y);
  }

  if (numRowsPerRestartInterval < 1)
    ThrowRDE("Number of rows per restart interval must be positives");

  // How many full pixel blocks will we produce?
  fullBlocks = tileRequiredWidth / frame.cps; // Truncating division!
  // Do we need to also produce part of a block?
  trailingPixels = tileRequiredWidth % frame.cps;
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

template <int N_COMP, bool WeirdWidth>
void LJpegDecompressor::decodeRowN(
    CroppedArray1DRef<uint16_t> outRow, std::array<uint16_t, N_COMP> pred,
    std::array<std::reference_wrapper<const PrefixCodeDecoder<>>, N_COMP> ht,
    BitStreamerJPEG& bs) const {
  // FIXME: predictor may have value outside of the uint16_t.
  // https://github.com/darktable-org/rawspeed/issues/175

  int col = 0;
  // For x, we first process all full pixel blocks within the image buffer ...
  for (; col < N_COMP * fullBlocks; col += N_COMP) {
    for (int i = 0; i != N_COMP; ++i) {
      pred[i] =
          uint16_t(pred[i] + (static_cast<const PrefixCodeDecoder<>&>(ht[i]))
                                 .decodeDifference(bs));
      outRow(col + i) = pred[i];
    }
  }

  // Sometimes we also need to consume one more block, and produce part of it.
  if /*constexpr*/ (WeirdWidth) {
    // FIXME: evaluate i-cache implications due to this being compile-time.
    static_assert(N_COMP > 1 || !WeirdWidth,
                  "can't want part of 1-pixel-wide block");
    // Some rather esoteric DNG's have odd dimensions, e.g. width % 2 = 1.
    // We may end up needing just part of last N_COMP pixels.
    invariant(trailingPixels > 0);
    invariant(trailingPixels < N_COMP);
    int c = 0;
    for (; c < trailingPixels; ++c) {
      pred[c] =
          uint16_t(pred[c] + (static_cast<const PrefixCodeDecoder<>&>(ht[c]))
                                 .decodeDifference(bs));
      outRow(col + c) = pred[c];
    }
    // Discard the rest of the block.
    invariant(c < N_COMP);
    for (; c < N_COMP; ++c) {
      (static_cast<const PrefixCodeDecoder<>&>(ht[c])).decodeDifference(bs);
    }
    col += N_COMP; // We did just process one more block.
  }

  // ... and discard the rest.
  for (; col < N_COMP * frame.dim.x; col += N_COMP) {
    for (int i = 0; i != N_COMP; ++i)
      (static_cast<const PrefixCodeDecoder<>&>(ht[i])).decodeDifference(bs);
  }
}

// N_COMP == number of components (2, 3 or 4)
template <int N_COMP, bool WeirdWidth>
ByteStream::size_type LJpegDecompressor::decodeN() const {
  invariant(mRaw->getCpp() > 0);
  invariant(N_COMP > 0);

  invariant(mRaw->dim.x >= N_COMP);
  invariant((mRaw->getCpp() * (mRaw->dim.x - imgFrame.pos.x)) >= N_COMP);

  const CroppedArray2DRef img(mRaw->getU16DataAsUncroppedArray2DRef(),
                              mRaw->getCpp() * imgFrame.pos.x, imgFrame.pos.y,
                              mRaw->getCpp() * imgFrame.dim.x, imgFrame.dim.y);

  const auto ht = getPrefixCodeDecoders<N_COMP>();

  // A recoded DNG might be split up into tiles of self contained LJpeg blobs.
  // The tiles at the bottom and the right may extend beyond the dimension of
  // the raw image buffer. The excessive content has to be ignored.

  invariant(frame.dim.y >= imgFrame.dim.y);
  invariant(static_cast<int64_t>(frame.cps) * frame.dim.x >=
            static_cast<int64_t>(mRaw->getCpp()) * imgFrame.dim.x);

  invariant(imgFrame.pos.y + imgFrame.dim.y <= mRaw->dim.y);
  invariant(imgFrame.pos.x + imgFrame.dim.x <= mRaw->dim.x);

  const auto numRestartIntervals = implicit_cast<int>(
      roundUpDivision(imgFrame.dim.y, numRowsPerRestartInterval));
  invariant(numRestartIntervals >= 0);
  invariant(numRestartIntervals != 0);

  ByteStream inputStream(DataBuffer(input, Endianness::little));
  for (int restartIntervalIndex = 0;
       restartIntervalIndex != numRestartIntervals; ++restartIntervalIndex) {
    auto pred = getInitialPreds<N_COMP>();
    auto predNext = Array1DRef(pred.data(), pred.size());

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

    for (int rowOfRestartInterval = 0;
         rowOfRestartInterval != numRowsPerRestartInterval;
         ++rowOfRestartInterval) {
      const int row = numRowsPerRestartInterval * restartIntervalIndex +
                      rowOfRestartInterval;
      invariant(row >= 0);
      invariant(row <= imgFrame.dim.y);

      // For y, we can simply stop decoding when we reached the border.
      if (row == imgFrame.dim.y) {
        invariant((restartIntervalIndex + 1) == numRestartIntervals);
        break;
      }

      auto outRow = img[row];
      copy_n(predNext.begin(), N_COMP, pred.data());
      // the predictor for the next line is the start of this line
      predNext = outRow
                     .getBlock(/*size=*/N_COMP,
                               /*index=*/0)
                     .getAsArray1DRef();

      decodeRowN<N_COMP, WeirdWidth>(outRow, pred, ht, bs);
    }

    inputStream.skipBytes(bs.getStreamPosition());
  }

  return inputStream.getPosition();
}

ByteStream::size_type LJpegDecompressor::decode() const {
  if (trailingPixels == 0) {
    switch (frame.cps) {
    case 1:
      return decodeN<1>();
    case 2:
      return decodeN<2>();
    case 3:
      return decodeN<3>();
    case 4:
      return decodeN<4>();
    default:
      __builtin_unreachable();
    }
  } else /* trailingPixels != 0 */ {
    // FIXME: using different function just for one tile likely causes
    // i-cache misses and whatnot. Need to check how not splitting it into
    // two different functions affects performance of the normal case.
    switch (frame.cps) {
    // Naturally can't happen for CPS=1.
    case 2:
      return decodeN<2, /*WeirdWidth=*/true>();
    case 3:
      return decodeN<3, /*WeirdWidth=*/true>();
    case 4:
      return decodeN<4, /*WeirdWidth=*/true>();
    default:
      __builtin_unreachable();
    }
  }
}

} // namespace rawspeed
