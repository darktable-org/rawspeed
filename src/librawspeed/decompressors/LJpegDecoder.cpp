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
#include "adt/Casts.h"
#include "adt/Invariant.h"
#include "adt/Point.h"
#include "common/RawImage.h"
#include "decoders/RawDecoderException.h"
#include "decompressors/AbstractLJpegDecoder.h"
#include "decompressors/LJpegDecompressor.h"
#include "io/Buffer.h"
#include "io/ByteStream.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <limits>
#include <memory>
#include <vector>

using std::copy_n;

namespace rawspeed {

LJpegDecoder::LJpegDecoder(ByteStream bs, const RawImage& img)
    : AbstractLJpegDecoder(bs, img) {
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
  if (mRaw->dim.x > 10240 || mRaw->dim.y > 7168) {
    ThrowRDE("Unexpected image dimensions found: (%i; %i)", mRaw->dim.x,
             mRaw->dim.y);
  }
#endif
}

void LJpegDecoder::decode(uint32_t offsetX, uint32_t offsetY, uint32_t width,
                          uint32_t height, iPoint2D maxDim_,
                          bool fixDng16Bug_) {
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

  if (!maxDim_.hasPositiveArea() ||
      implicit_cast<unsigned>(maxDim_.x) < width ||
      implicit_cast<unsigned>(maxDim_.y) < height)
    ThrowRDE("Requested tile is larger than tile's maximal dimensions");

  offX = offsetX;
  offY = offsetY;
  w = width;
  h = height;

  maxDim = maxDim_;

  fixDng16Bug = fixDng16Bug_;

  AbstractLJpegDecoder::decodeSOI();
}

Buffer::size_type LJpegDecoder::decodeScan() {
  invariant(frame.cps > 0);

  if (predictorMode < 1 || predictorMode > 7)
    ThrowRDE("Unsupported predictor mode: %u", predictorMode);

  for (uint32_t i = 0; i < frame.cps; i++)
    if (frame.compInfo[i].superH != 1 || frame.compInfo[i].superV != 1)
      ThrowRDE("Unsupported subsampling");

  const int N_COMP = frame.cps;

  std::vector<LJpegDecompressor::PerComponentRecipe> rec;
  rec.reserve(N_COMP);
  std::generate_n(std::back_inserter(rec), N_COMP,
                  [&rec, hts = getPrefixCodeDecoders(N_COMP),
                   initPred = getInitialPredictors(
                       N_COMP)]() -> LJpegDecompressor::PerComponentRecipe {
                    const auto i = implicit_cast<int>(rec.size());
                    return {*hts[i], initPred[i]};
                  });

  const auto jpegFrameDim = iPoint2D(frame.w, frame.h);

  if (implicit_cast<int64_t>(maxDim.x) * implicit_cast<int>(mRaw->getCpp()) >
      std::numeric_limits<int>::max())
    ThrowRDE("Maximal output tile is too large");

  const auto maxRes =
      iPoint2D(implicit_cast<int>(mRaw->getCpp()) * maxDim.x, maxDim.y);
  if (maxRes.area() != N_COMP * jpegFrameDim.area())
    ThrowRDE("LJpeg frame area does not match maximal tile area");

  // Detect whether the JPEG frame uses an inverted reshape (e.g. DJI/Blackmagic
  // CinemaDNG): JPEG frame is wider than tile and shorter, with packed rows.
  // Standard (Adobe): maxRes.x >= jpegFrameDim.x (tile is wider/equal)
  // Inverted (DJI):   jpegFrameDim.x > maxRes.x  (JPEG frame is wider)
  const bool invertedReshape = (jpegFrameDim.x > maxRes.x);
  if (!invertedReshape) {
    // Standard case: tile width is a multiple of JPEG frame width.
    if (maxRes.x % jpegFrameDim.x != 0 || maxRes.y % jpegFrameDim.y != 0)
      ThrowRDE(
          "Maximal output tile size is not a multiple of LJpeg frame size");

    const auto MCUSize =
        iPoint2D{maxRes.x / jpegFrameDim.x, maxRes.y / jpegFrameDim.y};
    if (MCUSize.area() != implicit_cast<uint64_t>(N_COMP))
      ThrowRDE("Unexpected MCU size, does not match LJpeg component count");

    // Standard MCU layouts have MCU.x >= MCU.y: {1,1}, {2,1}, {3,1},
    // {4,1}, {2,2}. If the MCU is purely vertical (e.g., {1,2}), the
    // encoder uses horizontal component interleaving with wider effective
    // JPEG rows that must be reshaped. Fall through to inverted reshape.
    if (MCUSize.x >= MCUSize.y) {
      const iRectangle2D imgFrame = {
          {static_cast<int>(offX), static_cast<int>(offY)},
          {static_cast<int>(w), static_cast<int>(h)}};
      const LJpegDecompressor::Frame jpegFrame = {MCUSize, jpegFrameDim};

      int numLJpegRowsPerRestartInterval;
      if (numMCUsPerRestartInterval == 0) {
        numLJpegRowsPerRestartInterval = jpegFrameDim.y;
      } else {
        const int numMCUsPerRow = jpegFrameDim.x;
        if (numMCUsPerRestartInterval % numMCUsPerRow != 0)
          ThrowRDE("Restart interval is not a multiple of frame row size");
        numLJpegRowsPerRestartInterval =
            numMCUsPerRestartInterval / numMCUsPerRow;
      }

      LJpegDecompressor d(mRaw, imgFrame, jpegFrame, rec,
                          numLJpegRowsPerRestartInterval,
                          implicit_cast<int>(predictorMode),
                          input.peekRemainingBuffer().getAsArray1DRef());
      return d.decode();
    }
  }

  // Inverted reshape case:
  // The effective decoded width per JPEG row exceeds the tile width.
  // This occurs when:
  //   (a) The JPEG frame is wider than the tile (DJI/Blackmagic CinemaDNG):
  //       e.g., JPEG=8000x1500 1-comp, tile=4000x3000.
  //   (b) Multi-component JPEG with vertical MCU ratio:
  //       e.g., JPEG=592x79 2-comp, tile=592x158 (effectiveWidth=1184).
  // In both cases, components are interleaved horizontally, and each JPEG row
  // contains 'widthPack' tile rows of pixel data concatenated.
  const int effectiveJpegWidth = jpegFrameDim.x * N_COMP;

  if (effectiveJpegWidth % maxRes.x != 0)
    ThrowRDE("Effective JPEG width is not a multiple of tile width");
  if (maxRes.y % jpegFrameDim.y != 0)
    ThrowRDE("Tile height is not a multiple of LJpeg frame height");

  const int widthPack = effectiveJpegWidth / maxRes.x;
  if (widthPack * jpegFrameDim.y != maxRes.y)
    ThrowRDE("Inverted reshape dimensions mismatch");

  if (widthPack < 1 || widthPack > 4)
    ThrowRDE("Unexpected row packing factor: %d", widthPack);

  // Decode into a temporary buffer at effective decoded dimensions.
  // Components are always interleaved horizontally: MCU{N_COMP, 1}.
  const auto MCUSize = iPoint2D{N_COMP, 1};

  // Create a temporary raw image to decode the JPEG into.
  // RawImage::create with dimensions already calls createData() internally.
  RawImage tmpRaw = RawImage::create(
      iPoint2D(effectiveJpegWidth, jpegFrameDim.y), RawImageType::UINT16, 1);

  const iRectangle2D tmpFrame = {{0, 0}, {effectiveJpegWidth, jpegFrameDim.y}};
  const LJpegDecompressor::Frame jpegFrame = {MCUSize, jpegFrameDim};

  int numLJpegRowsPerRestartInterval;
  if (numMCUsPerRestartInterval == 0) {
    numLJpegRowsPerRestartInterval = jpegFrameDim.y;
  } else {
    const int numMCUsPerRow = jpegFrameDim.x;
    if (numMCUsPerRestartInterval % numMCUsPerRow != 0)
      ThrowRDE("Restart interval is not a multiple of frame row size");
    numLJpegRowsPerRestartInterval = numMCUsPerRestartInterval / numMCUsPerRow;
  }

  LJpegDecompressor d(tmpRaw, tmpFrame, jpegFrame, rec,
                      numLJpegRowsPerRestartInterval,
                      implicit_cast<int>(predictorMode),
                      input.peekRemainingBuffer().getAsArray1DRef());
  const auto consumed = d.decode();

  // Deinterleave: each JPEG row of width (widthPack * tileW) maps to
  // widthPack consecutive tile rows of width tileW.
  const auto tmpData = tmpRaw->getU16DataAsUncroppedArray2DRef();
  const auto outData = mRaw->getU16DataAsUncroppedArray2DRef();

  const auto tileW = implicit_cast<int>(w);
  const auto cpp = implicit_cast<int>(mRaw->getCpp());
  const int outRowPixels = cpp * tileW;

  for (int jpegRow = 0; jpegRow < jpegFrameDim.y; ++jpegRow) {
    for (int pack = 0; pack < widthPack; ++pack) {
      const int tileRow = implicit_cast<int>(offY) + jpegRow * widthPack + pack;
      if (tileRow >= mRaw->dim.y)
        continue;
      const int srcCol = pack * outRowPixels;
      const int dstCol = cpp * implicit_cast<int>(offX);
      // Contiguous row-segment copy. Bounds guaranteed by validation:
      // srcCol + outRowPixels <= widthPack * outRowPixels <= effectiveJpegWidth
      std::memcpy(&outData(tileRow, dstCol), &tmpData(jpegRow, srcCol),
                  sizeof(uint16_t) * outRowPixels);
    }
  }

  return consumed;
}

} // namespace rawspeed
