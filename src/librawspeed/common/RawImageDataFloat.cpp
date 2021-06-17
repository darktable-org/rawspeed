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

#include "common/RawImage.h"              // for RawImageDataFloat, TYPE_FL...
#include "common/Common.h"                // for writeLog, DEBUG_PRIO_INFO
#include "common/Point.h"                 // for iPoint2D
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "metadata/BlackArea.h"           // for BlackArea
#include <algorithm>                      // for max, min
#include <array>                          // for array
#include <cstdint>                        // for uint8_t, uint32_t, uint16_t
#include <memory>                         // for operator==, unique_ptr
#include <vector>                         // for vector

using std::min;
using std::max;

namespace rawspeed {

RawImageDataFloat::RawImageDataFloat() {
  bpp = sizeof(float);
  dataType = TYPE_FLOAT32;
  }

  RawImageDataFloat::RawImageDataFloat(const iPoint2D& _dim, uint32_t _cpp)
      : RawImageData(_dim, sizeof(float), _cpp) {
    dataType = TYPE_FLOAT32;
  }

  void RawImageDataFloat::calculateBlackAreas() {
    std::array<float, 4> accPixels;
    accPixels.fill(0);
    int totalpixels = 0;

    for (auto area : blackAreas) {
      /* Make sure area sizes are multiple of two,
      so we have the same amount of pixels for each CFA group */
      area.size = area.size - (area.size&1);

      /* Process horizontal area */
      if (!area.isVertical) {
        if (static_cast<int>(area.offset) + static_cast<int>(area.size) >
            uncropped_dim.y)
          ThrowRDE("Offset + size is larger than height of image");
        for (uint32_t y = area.offset; y < area.offset + area.size; y++) {
          auto* pixel =
              reinterpret_cast<float*>(getDataUncropped(mOffset.x, y));

          for (int x = mOffset.x; x < dim.x + mOffset.x; x++) {
            accPixels[((y & 1) << 1) | (x & 1)] += *pixel;
            pixel++;
          }
        }
        totalpixels += area.size * dim.x;
      }

      /* Process vertical area */
      if (area.isVertical) {
        if (static_cast<int>(area.offset) + static_cast<int>(area.size) >
            uncropped_dim.x)
          ThrowRDE("Offset + size is larger than width of image");
        for (int y = mOffset.y; y < dim.y+mOffset.y; y++) {
          auto* pixel =
              reinterpret_cast<float*>(getDataUncropped(area.offset, y));

          for (uint32_t x = area.offset; x < area.size + area.offset; x++) {
            accPixels[((y & 1) << 1) | (x & 1)] += *pixel;
            pixel++;
          }
        }
        totalpixels += area.size * dim.y;
      }
    }

    if (!totalpixels) {
      for (int &i : blackLevelSeparate)
        i = blackLevel;
      return;
    }

    /* Calculate median value of black areas for each component */
    /* Adjust the number of total pixels so it is the same as the median of each histogram */
    totalpixels /= 4;

    for (int i = 0 ; i < 4; i++) {
      blackLevelSeparate[i] =
          static_cast<int>(65535.0F * accPixels[i] / totalpixels);
    }

    /* If this is not a CFA image, we do not use separate blacklevels, use average */
    if (!isCFA) {
      int total = 0;
      for (int i : blackLevelSeparate)
        total += i;
      for (int &i : blackLevelSeparate)
        i = (total + 2) >> 2;
    }
  }

  void RawImageDataFloat::scaleBlackWhite() {
    const int skipBorder = 150;
    int gw = (dim.x - skipBorder) * cpp;
    if ((blackAreas.empty() && blackLevelSeparate[0] < 0 && blackLevel < 0) || whitePoint == 65536) {  // Estimate
      float b = 100000000;
      float m = -10000000;
      for (int row = skipBorder*cpp;row < (dim.y - skipBorder);row++) {
        auto* pixel = reinterpret_cast<float*>(getData(skipBorder, row));
        for (int col = skipBorder ; col < gw ; col++) {
          b = min(*pixel, b);
          m = max(*pixel, m);
          pixel++;
        }
      }
      if (blackLevel < 0)
        blackLevel = static_cast<int>(b);
      if (whitePoint == 65536)
        whitePoint = static_cast<int>(m);
      writeLog(DEBUG_PRIO_INFO, "Estimated black:%d, Estimated white: %d",
               blackLevel, whitePoint);
    }

    /* If filter has not set separate blacklevel, compute or fetch it */
    if (blackLevelSeparate[0] < 0)
      calculateBlackAreas();

    startWorker(RawImageWorker::SCALE_VALUES, true);
}

void RawImageDataFloat::scaleValues(int start_y, int end_y) {
  int gw = dim.x * cpp;
  std::array<float, 4> mul;
  std::array<float, 4> sub;
  for (int i = 0; i < 4; i++) {
    int v = i;
    if ((mOffset.x & 1) != 0)
      v ^= 1;
    if ((mOffset.y & 1) != 0)
      v ^= 2;
    mul[i] = 65535.0F / static_cast<float>(whitePoint - blackLevelSeparate[v]);
    sub[i] = static_cast<float>(blackLevelSeparate[v]);
  }
  for (int y = start_y; y < end_y; y++) {
    auto* pixel = reinterpret_cast<float*>(getData(0, y));
    float* mul_local = &mul[2 * (y & 1)];
    float* sub_local = &sub[2 * (y & 1)];
    for (int x = 0; x < gw; x++) {
      pixel[x] = (pixel[x] - sub_local[x & 1]) * mul_local[x & 1];
    }
  }
}

  /* This performs a 4 way interpolated pixel */
  /* The value is interpolated from the 4 closest valid pixels in */
  /* the horizontal and vertical direction. Pixels found further away */
  /* are weighed less */

void RawImageDataFloat::fixBadPixel(uint32_t x, uint32_t y, int component) {
  std::array<float, 4> values;
  values.fill(-1);
  std::array<float, 4> dist = {{}};
  std::array<float, 4> weight;

  uint8_t* bad_line = &mBadPixelMap[y * mBadPixelMapPitch];
  // We can have cfa or no-cfa for RawImageDataFloat
  int step = isCFA ? 2 : 1;

  // Find pixel to the left
  int x_find = static_cast<int>(x) - step;
  int curr = 0;
  while (x_find >= 0 && values[curr] < 0) {
    if (0 == ((bad_line[x_find>>3] >> (x_find&7)) & 1)) {
      values[curr] = (reinterpret_cast<float*>(getDataUncropped(x_find, y)))[component];
      dist[curr] = static_cast<float>(static_cast<int>(x) - x_find);
    }
    x_find -= step;
  }
  // Find pixel to the right
  x_find = static_cast<int>(x) + step;
  curr = 1;
  while (x_find < uncropped_dim.x && values[curr] < 0) {
    if (0 == ((bad_line[x_find>>3] >> (x_find&7)) & 1)) {
      values[curr] = (reinterpret_cast<float*>(getDataUncropped(x_find, y)))[component];
      dist[curr] = static_cast<float>(x_find - static_cast<int>(x));
    }
    x_find += step;
  }

  bad_line = &mBadPixelMap[x>>3];
  // Find pixel upwards
  int y_find = static_cast<int>(y) - step;
  curr = 2;
  while (y_find >= 0 && values[curr] < 0) {
    if (0 == ((bad_line[y_find*mBadPixelMapPitch] >> (x&7)) & 1)) {
      values[curr] = (reinterpret_cast<float*>(getDataUncropped(x, y_find)))[component];
      dist[curr] = static_cast<float>(static_cast<int>(y) - y_find);
    }
    y_find -= step;
  }
  // Find pixel downwards
  y_find = static_cast<int>(y) + step;
  curr = 3;
  while (y_find < uncropped_dim.y && values[curr] < 0) {
    if (0 == ((bad_line[y_find*mBadPixelMapPitch] >> (x&7)) & 1)) {
      values[curr] = (reinterpret_cast<float*>(getDataUncropped(x, y_find)))[component];
      dist[curr] = static_cast<float>(y_find - static_cast<int>(y));
    }
    y_find += step;
  }
  // Find x weights
  float total_dist_x = dist[0] + dist[1];

  float total_div = 0.000001F;
  if (total_dist_x) {
    weight[0] = dist[0] > 0.0F ? (total_dist_x - dist[0]) / total_dist_x : 0;
    weight[1] = 1.0F - weight[0];
    total_div += 1;
  }

  // Find y weights
  float total_dist_y = dist[2] + dist[3];
  if (total_dist_y) {
    weight[2] = dist[2] > 0.0F ? (total_dist_y - dist[2]) / total_dist_y : 0;
    weight[3] = 1.0F - weight[2];
    total_div += 1;
  }


  float total_pixel = 0;
  for (int i = 0; i < 4; i++)
    if (values[i] >= 0)
      total_pixel += values[i] * weight[i];

  total_pixel /= total_div;
  auto* pix = reinterpret_cast<float*>(getDataUncropped(x, y));
  pix[component] = total_pixel;

  /* Process other pixels - could be done inline, since we have the weights */
  if (cpp > 1 && component == 0)
    for (int i = 1; i < static_cast<int>(cpp); i++)
      fixBadPixel(x,y,i);
}

void RawImageDataFloat::doLookup( int start_y, int end_y ) {
  ThrowRDE("Float point lookup tables not implemented");
}

void RawImageDataFloat::setWithLookUp(uint16_t value, uint8_t* dst,
                                      uint32_t* random) {
  auto* dest = reinterpret_cast<float*>(dst);
  if (table == nullptr) {
    *dest = static_cast<float>(value) * (1.0F / 65535);
    return;
  }

  ThrowRDE("Float point lookup tables not implemented");
}

} // namespace rawspeed
