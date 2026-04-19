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
#include <cstdint>
#include <cstring>
#include <iterator>
#include <limits>
#include <vector>

namespace rawspeed {

namespace {

using PerCompRecipeVec = std::vector<LJpegDecompressor::PerComponentRecipe>;

struct ScanSettings final {
  RawImage raw;
  iRectangle2D imgFrame;
  iPoint2D jpegFrameDim;
  iPoint2D maxRes;
  LJpegDecompressor::DecodeSettings decode;
  Array1DRef<const uint8_t> input;
};

[[nodiscard]] int
getNumLJpegRowsPerRestartInterval(uint32_t numMCUsPerRestartInterval,
                                  iPoint2D jpegFrameDim) {
  if (numMCUsPerRestartInterval == 0)
    return jpegFrameDim.y;

  const int numMCUsPerRow = jpegFrameDim.x;
  if (numMCUsPerRestartInterval % numMCUsPerRow != 0)
    ThrowRDE("Restart interval is not a multiple of frame row size");
  return implicit_cast<int>(numMCUsPerRestartInterval) / numMCUsPerRow;
}

[[nodiscard]] iPoint2D getMaxResolution(const RawImage& raw, iPoint2D maxDim,
                                        int numComponents,
                                        iPoint2D jpegFrameDim) {
  if (implicit_cast<int64_t>(maxDim.x) * implicit_cast<int>(raw->getCpp()) >
      std::numeric_limits<int>::max())
    ThrowRDE("Maximal output tile is too large");

  const auto maxRes =
      iPoint2D(implicit_cast<int>(raw->getCpp()) * maxDim.x, maxDim.y);
  if (maxRes.area() != numComponents * jpegFrameDim.area())
    ThrowRDE("LJpeg frame area does not match maximal tile area");

  return maxRes;
}

[[nodiscard]] iPoint2D
getStandardMCUSize(int numComponents, iPoint2D jpegFrameDim, iPoint2D maxRes) {
  if (jpegFrameDim.x > maxRes.x)
    return {};

  if (maxRes.x % jpegFrameDim.x != 0 || maxRes.y % jpegFrameDim.y != 0)
    ThrowRDE("Maximal output tile size is not a multiple of LJpeg frame size");

  const auto mcuSize =
      iPoint2D{maxRes.x / jpegFrameDim.x, maxRes.y / jpegFrameDim.y};
  if (mcuSize.area() != implicit_cast<uint64_t>(numComponents))
    ThrowRDE("Unexpected MCU size, does not match LJpeg component count");

  if (mcuSize.x < mcuSize.y)
    return {};

  return mcuSize;
}

[[nodiscard]] ByteStream::size_type
decodeStandardScan(const ScanSettings& settings, iPoint2D mcuSize,
                   const PerCompRecipeVec& rec) {
  const LJpegDecompressor::Frame jpegFrame = {mcuSize, settings.jpegFrameDim};
  LJpegDecompressor d(settings.raw, settings.imgFrame, jpegFrame, rec,
                      settings.decode, settings.input);
  return d.decode();
}

void copyDeinterleavedRows(const RawImage& raw, RawImage tmpRaw, uint32_t offX,
                           uint32_t offY, uint32_t tileWidth, int widthPack) {
  const auto tmpData = tmpRaw->getU16DataAsUncroppedArray2DRef();
  const auto outData = raw->getU16DataAsUncroppedArray2DRef();

  const auto cpp = implicit_cast<int>(raw->getCpp());
  const int outRowPixels = cpp * implicit_cast<int>(tileWidth);

  for (int jpegRow = 0; jpegRow < tmpRaw->dim.y; ++jpegRow) {
    for (int pack = 0; pack < widthPack; ++pack) {
      const int tileRow = implicit_cast<int>(offY) + jpegRow * widthPack + pack;
      if (tileRow >= raw->dim.y)
        continue;

      const int srcCol = pack * outRowPixels;
      const int dstCol = cpp * implicit_cast<int>(offX);
      std::memcpy(&outData(tileRow, dstCol), &tmpData(jpegRow, srcCol),
                  sizeof(uint16_t) * outRowPixels);
    }
  }
}

[[nodiscard]] ByteStream::size_type
decodeInvertedScan(const ScanSettings& settings, int numComponents,
                   uint32_t offX, uint32_t offY, uint32_t tileWidth,
                   const PerCompRecipeVec& rec) {
  const int effectiveJpegWidth = settings.jpegFrameDim.x * numComponents;

  if (effectiveJpegWidth % settings.maxRes.x != 0)
    ThrowRDE("Effective JPEG width is not a multiple of tile width");
  if (settings.maxRes.y % settings.jpegFrameDim.y != 0)
    ThrowRDE("Tile height is not a multiple of LJpeg frame height");

  const int widthPack = effectiveJpegWidth / settings.maxRes.x;
  if (widthPack * settings.jpegFrameDim.y != settings.maxRes.y)
    ThrowRDE("Inverted reshape dimensions mismatch");
  if (widthPack < 1 || widthPack > 4)
    ThrowRDE("Unexpected row packing factor: %d", widthPack);

  const auto mcuSize = iPoint2D{numComponents, 1};
  RawImage tmpRaw =
      RawImage::create(iPoint2D(effectiveJpegWidth, settings.jpegFrameDim.y),
                       RawImageType::UINT16, 1);
  const iRectangle2D tmpFrame = {{0, 0},
                                 {effectiveJpegWidth, settings.jpegFrameDim.y}};
  const LJpegDecompressor::Frame jpegFrame = {mcuSize, settings.jpegFrameDim};

  LJpegDecompressor d(tmpRaw, tmpFrame, jpegFrame, rec, settings.decode,
                      settings.input);
  const auto consumed = d.decode();

  copyDeinterleavedRows(settings.raw, tmpRaw, offX, offY, tileWidth, widthPack);
  return consumed;
}

} // namespace

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

  const int numComponents = frame.cps;

  PerCompRecipeVec rec;
  rec.reserve(numComponents);
  std::generate_n(std::back_inserter(rec), numComponents,
                  [&rec, hts = getPrefixCodeDecoders(numComponents),
                   initPred = getInitialPredictors(numComponents)]()
                      -> LJpegDecompressor::PerComponentRecipe {
                    const auto i = implicit_cast<int>(rec.size());
                    return {*hts[i], initPred[i]};
                  });

  const auto jpegFrameDim = iPoint2D(frame.w, frame.h);
  const auto maxRes =
      getMaxResolution(mRaw, maxDim, numComponents, jpegFrameDim);
  const ScanSettings settings{mRaw,
                              {{static_cast<int>(offX), static_cast<int>(offY)},
                               {static_cast<int>(w), static_cast<int>(h)}},
                              jpegFrameDim,
                              maxRes,
                              {getNumLJpegRowsPerRestartInterval(
                                   numMCUsPerRestartInterval, jpegFrameDim),
                               implicit_cast<int>(predictorMode)},
                              input.peekRemainingBuffer().getAsArray1DRef()};

  if (const auto mcuSize =
          getStandardMCUSize(numComponents, jpegFrameDim, maxRes);
      mcuSize.hasPositiveArea())
    return decodeStandardScan(settings, mcuSize, rec);

  return decodeInvertedScan(settings, numComponents, offX, offY, w, rec);
}

} // namespace rawspeed
