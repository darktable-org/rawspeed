/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post

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

#include "common/RawImage.h"
#include "adt/Array1DRef.h"
#include "adt/Array2DRef.h"
#include "adt/Casts.h"
#include "adt/CroppedArray2DRef.h"
#include "adt/Point.h"
#include "common/Common.h"
#include "decoders/RawDecoderException.h"
#include "metadata/BlackArea.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vector>

using std::max;
using std::min;

namespace rawspeed {

RawImageDataFloat::RawImageDataFloat() {
  bpp = sizeof(float);
  dataType = RawImageType::F32;
}

RawImageDataFloat::RawImageDataFloat(const iPoint2D& _dim, uint32_t _cpp)
    : RawImageData(RawImageType::F32, _dim, sizeof(float), _cpp) {}

void RawImageDataFloat::calculateBlackAreas() {
  const Array2DRef<float> img = getF32DataAsUncroppedArray2DRef();

  std::array<float, 4> accPixels;
  accPixels.fill(0);
  int totalpixels = 0;

  for (auto area : blackAreas) {
    /* Make sure area sizes are multiple of two,
    so we have the same amount of pixels for each CFA group */
    area.size = area.size - (area.size & 1);

    /* Process horizontal area */
    if (!area.isVertical) {
      if (static_cast<int>(area.offset) + static_cast<int>(area.size) >
          uncropped_dim.y)
        ThrowRDE("Offset + size is larger than height of image");
      for (uint32_t y = area.offset; y < area.offset + area.size; y++) {
        for (int x = mOffset.x; x < dim.x + mOffset.x; x++) {
          accPixels[((y & 1) << 1) | (x & 1)] += img(y, x);
        }
      }
      totalpixels += area.size * dim.x;
    }

    /* Process vertical area */
    if (area.isVertical) {
      if (static_cast<int>(area.offset) + static_cast<int>(area.size) >
          uncropped_dim.x)
        ThrowRDE("Offset + size is larger than width of image");
      for (int y = mOffset.y; y < dim.y + mOffset.y; y++) {
        for (uint32_t x = area.offset; x < area.size + area.offset; x++) {
          accPixels[((y & 1) << 1) | (x & 1)] += img(y, x);
        }
      }
      totalpixels += area.size * dim.y;
    }
  }

  blackLevelSeparate = Array2DRef(blackLevelSeparateStorage.data(), 2, 2);
  auto blackLevelSeparate1D = *blackLevelSeparate->getAsArray1DRef();

  if (!totalpixels) {
    for (int& i : blackLevelSeparate1D)
      i = blackLevel;
    return;
  }

  /* Calculate median value of black areas for each component */
  /* Adjust the number of total pixels so it is the same as the median of each
   * histogram */
  totalpixels /= 4;

  for (int i = 0; i < 4; i++) {
    blackLevelSeparate1D(i) = static_cast<int>(
        65535.0F * accPixels[i] / implicit_cast<float>(totalpixels));
  }

  /* If this is not a CFA image, we do not use separate blacklevels, use average
   */
  if (!isCFA) {
    int total = 0;
    for (int i : blackLevelSeparate1D)
      total += i;
    for (int& i : blackLevelSeparate1D)
      i = (total + 2) >> 2;
  }
}

void RawImageDataFloat::scaleBlackWhite() {
  const CroppedArray2DRef<float> img = getF32DataAsCroppedArray2DRef();

  const int skipBorder = 150;
  int gw = (dim.x - skipBorder) * cpp;
  // NOTE: lack of whitePoint means that it is pre-normalized.
  if (blackAreas.empty() && !blackLevelSeparate && blackLevel < 0) { // Estimate
    float b = 100000000;
    float m = -10000000;
    for (int row = skipBorder * cpp; row < (dim.y - skipBorder); row++) {
      for (int col = skipBorder; col < gw; col++) {
        const float pixel = img(row, col);
        b = min(pixel, b);
        m = max(pixel, m);
      }
    }
    if (blackLevel < 0)
      blackLevel = static_cast<int>(b);
    writeLog(DEBUG_PRIO::INFO, "Estimated black:%d", blackLevel);
  }

  /* If filter has not set separate blacklevel, compute or fetch it */
  if (!blackLevelSeparate)
    calculateBlackAreas();

  startWorker(RawImageWorker::RawImageWorkerTask::SCALE_VALUES, true);
}

void RawImageDataFloat::scaleValues(int start_y, int end_y) {
  const CroppedArray2DRef<float> img = getF32DataAsCroppedArray2DRef();
  int gw = dim.x * cpp;
  std::array<float, 4> mul;
  std::array<float, 4> sub;
  assert(blackLevelSeparate->width() == 2 && blackLevelSeparate->height() == 2);
  auto blackLevelSeparate1D = *blackLevelSeparate->getAsArray1DRef();
  for (int i = 0; i < 4; i++) {
    int v = i;
    if ((mOffset.x & 1) != 0)
      v ^= 1;
    if ((mOffset.y & 1) != 0)
      v ^= 2;
    mul[i] =
        65535.0F / static_cast<float>(*whitePoint - blackLevelSeparate1D(v));
    sub[i] = static_cast<float>(blackLevelSeparate1D(v));
  }
  for (int y = start_y; y < end_y; y++) {
    for (int x = 0; x < gw; x++)
      img(y, x) = (img(y, x) - sub[(2 * (y & 1)) + (x & 1)]) *
                  mul[(2 * (y & 1)) + (x & 1)];
  }
}

/* This performs a 4 way interpolated pixel */
/* The value is interpolated from the 4 closest valid pixels in */
/* the horizontal and vertical direction. Pixels found further away */
/* are weighed less */

void RawImageDataFloat::fixBadPixel(uint32_t x, uint32_t y, int component) {
  const Array2DRef<float> img = getF32DataAsUncroppedArray2DRef();

  std::array<float, 4> values;
  values.fill(-1);
  std::array<float, 4> dist = {{}};
  std::array<float, 4> weight;

  const auto bad =
      Array2DRef(mBadPixelMap.data(), mBadPixelMapPitch, uncropped_dim.y);
  // We can have cfa or no-cfa for RawImageDataFloat
  int step = isCFA ? 2 : 1;

  // Find pixel to the left
  int x_find = static_cast<int>(x) - step;
  int curr = 0;
  while (x_find >= 0 && values[curr] < 0) {
    if (0 == ((bad(y, x_find >> 3) >> (x_find & 7)) & 1)) {
      values[curr] = img(y, x_find + component);
      dist[curr] = static_cast<float>(static_cast<int>(x) - x_find);
    }
    x_find -= step;
  }
  // Find pixel to the right
  x_find = static_cast<int>(x) + step;
  curr = 1;
  while (x_find < uncropped_dim.x && values[curr] < 0) {
    if (0 == ((bad(y, x_find >> 3) >> (x_find & 7)) & 1)) {
      values[curr] = img(y, x_find + component);
      dist[curr] = static_cast<float>(x_find - static_cast<int>(x));
    }
    x_find += step;
  }

  // Find pixel upwards
  int y_find = static_cast<int>(y) - step;
  curr = 2;
  while (y_find >= 0 && values[curr] < 0) {
    if (0 == ((bad(y_find, x >> 3) >> (x & 7)) & 1)) {
      values[curr] = img(y_find, x + component);
      dist[curr] = static_cast<float>(static_cast<int>(y) - y_find);
    }
    y_find -= step;
  }
  // Find pixel downwards
  y_find = static_cast<int>(y) + step;
  curr = 3;
  while (y_find < uncropped_dim.y && values[curr] < 0) {
    if (0 == ((bad(y_find, x >> 3) >> (x & 7)) & 1)) {
      values[curr] = img(y_find, x + component);
      dist[curr] = static_cast<float>(y_find - static_cast<int>(y));
    }
    y_find += step;
  }

  float total_div = 0.000001F;

  // Find x weights
  if (float total_dist_x = dist[0] + dist[1]; std::abs(total_dist_x) > 0) {
    weight[0] = dist[0] > 0.0F ? (total_dist_x - dist[0]) / total_dist_x : 0;
    weight[1] = 1.0F - weight[0];
    total_div += 1;
  }

  // Find y weights
  if (float total_dist_y = dist[2] + dist[3]; std::abs(total_dist_y) > 0) {
    weight[2] = dist[2] > 0.0F ? (total_dist_y - dist[2]) / total_dist_y : 0;
    weight[3] = 1.0F - weight[2];
    total_div += 1;
  }

  float total_pixel = 0;
  for (int i = 0; i < 4; i++)
    if (values[i] >= 0)
      total_pixel += values[i] * weight[i];

  total_pixel /= total_div;
  img(y, x + component) = total_pixel;

  /* Process other pixels - could be done inline, since we have the weights */
  if (cpp > 1 && component == 0)
    for (int i = 1; i < cpp; i++)
      fixBadPixel(x, y, i);
}

void RawImageDataFloat::doLookup(int start_y, int end_y) {
  ThrowRDE("Float point lookup tables not implemented");
}

void RawImageDataFloat::setWithLookUp(uint16_t value, std::byte* dst,
                                      uint32_t* random) {
  auto* dest = reinterpret_cast<float*>(dst);
  if (table == nullptr) {
    *dest = static_cast<float>(value) * (1.0F / 65535);
    return;
  }

  ThrowRDE("Float point lookup tables not implemented");
}

} // namespace rawspeed
