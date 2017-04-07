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

#include "common/RawImage.h"              // for RawImageDataU16, TableLookUp
#include "common/Common.h"                // for ushort16, uint32, uchar8
#include "common/Memory.h"                // for alignedFree, alignedMalloc...
#include "common/Point.h"                 // for iPoint2D
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "metadata/BlackArea.h"           // for BlackArea
#include <algorithm>                      // for fill, max, min
#include <array>                          // for array
#include <cassert>                        // for assert
#include <vector>                         // for vector

#if defined(__SSE2__)
#include "common/Cpuid.h" // for Cpuid
#include <emmintrin.h> // for __m128i, _mm_load_si128
#include <xmmintrin.h> // for _MM_HINT_T0, _mm_prefetch
#endif

using std::vector;
using std::min;
using std::max;
using std::array;

namespace rawspeed {

RawImageDataU16::RawImageDataU16() {
  dataType = TYPE_USHORT16;
  bpp = 2;
}

RawImageDataU16::RawImageDataU16(const iPoint2D &_dim, uint32 _cpp)
    : RawImageData(_dim, 2, _cpp) {
  dataType = TYPE_USHORT16;
}


void RawImageDataU16::calculateBlackAreas() {
  vector<unsigned int> histogram(4 * 65536);
  fill(histogram.begin(), histogram.end(), 0);

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
      for (uint32 y = area.offset; y < area.offset+area.size; y++) {
        auto* pixel =
            reinterpret_cast<ushort16*>(getDataUncropped(mOffset.x, y));
        auto* localhist = &histogram[(y & 1) * (65536UL * 2UL)];
        for (int x = mOffset.x; x < dim.x+mOffset.x; x++) {
          localhist[((x&1)<<16) + *pixel]++;
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
            reinterpret_cast<ushort16*>(getDataUncropped(area.offset, y));
        auto* localhist = &histogram[(y & 1) * (65536UL * 2UL)];
        for (uint32 x = area.offset; x < area.size+area.offset; x++) {
          localhist[((x&1)<<16) + *pixel]++;
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
  totalpixels /= 4*2;

  for (int i = 0 ; i < 4; i++) {
    auto* localhist = &histogram[i * 65536UL];
    int acc_pixels = localhist[0];
    int pixel_value = 0;
    while (acc_pixels <= totalpixels && pixel_value < 65535) {
      pixel_value++;
      acc_pixels += localhist[pixel_value];
    }
    blackLevelSeparate[i] = pixel_value;
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

void RawImageDataU16::scaleBlackWhite() {
  const int skipBorder = 250;
  int gw = (dim.x - skipBorder) * cpp;
  if ((blackAreas.empty() && blackLevelSeparate[0] < 0 && blackLevel < 0) || whitePoint >= 65536) {  // Estimate
    int b = 65536;
    int m = 0;
    for (int row = skipBorder; row < (dim.y - skipBorder);row++) {
      auto* pixel = reinterpret_cast<ushort16*>(getData(skipBorder, row));
      for (int col = skipBorder ; col < gw ; col++) {
        b = min(static_cast<int>(*pixel), b);
        m = max(static_cast<int>(*pixel), m);
        pixel++;
      }
    }
    if (blackLevel < 0)
      blackLevel = b;
    if (whitePoint >= 65536)
      whitePoint = m;
    writeLog(DEBUG_PRIO_INFO, "ISO:%d, Estimated black:%d, Estimated white: %d",
             metadata.isoSpeed, blackLevel, whitePoint);
  }

  /* Skip, if not needed */
  if ((blackAreas.empty() && blackLevel == 0 && whitePoint == 65535 &&
       blackLevelSeparate[0] < 0) ||
      dim.area() <= 0)
    return;

  /* If filter has not set separate blacklevel, compute or fetch it */
  if (blackLevelSeparate[0] < 0)
    calculateBlackAreas();

  startWorker(RawImageWorker::SCALE_VALUES, true);
}

void RawImageDataU16::scaleValues(int start_y, int end_y) {
#if !((defined(_MSC_VER) && _MSC_VER > 1399) || defined(__SSE2__))

  return scaleValues_plain(start_y, end_y);

#else

  int depth_values = whitePoint - blackLevelSeparate[0];
  float app_scale = 65535.0f / depth_values;

  // Check SSE2
  if (Cpuid::SSE2() && app_scale < 63) {
    scaleValues_SSE2(start_y, end_y);
  } else {
    scaleValues_plain(start_y, end_y);
  }

#endif
}

#if (defined(_MSC_VER) && _MSC_VER > 1399) || defined(__SSE2__)
void RawImageDataU16::scaleValues_SSE2(int start_y, int end_y) {
  int depth_values = whitePoint - blackLevelSeparate[0];
  float app_scale = 65535.0f / depth_values;

  // Scale in 30.2 fp
  auto full_scale_fp = static_cast<int>(app_scale * 4.0f);
  // Half Scale in 18.14 fp
  auto half_scale_fp = static_cast<int>(app_scale * 4095.0f);

  __m128i sseround;
  __m128i ssesub2;
  __m128i ssesign;
  __m128i rand_mul;
  __m128i rand_mask;
  __m128i sse_full_scale_fp;
  __m128i sse_half_scale_fp;

  auto* sub_mul = alignedMallocArray<uint32, 16, __m128i>(4);
  if (!sub_mul)
    ThrowRDE("Out of memory, failed to allocate 128 bytes");

  assert(sub_mul != nullptr);

  uint32 gw = pitch / 16;
  // 10 bit fraction
  uint32 mul = static_cast<int>(
      1024.0f * 65535.0f /
      static_cast<float>(whitePoint - blackLevelSeparate[mOffset.x & 1]));
  mul |= (static_cast<int>(
             1024.0f * 65535.0f /
             static_cast<float>(whitePoint -
                                blackLevelSeparate[(mOffset.x + 1) & 1])))
         << 16;
  uint32 b = blackLevelSeparate[mOffset.x & 1] |
             (blackLevelSeparate[(mOffset.x + 1) & 1] << 16);

  for (int i = 0; i < 4; i++) {
    sub_mul[i] = b;       // Subtract even lines
    sub_mul[4 + i] = mul; // Multiply even lines
  }

  mul = static_cast<int>(
      1024.0f * 65535.0f /
      static_cast<float>(whitePoint - blackLevelSeparate[2 + (mOffset.x & 1)]));
  mul |= (static_cast<int>(
             1024.0f * 65535.0f /
             static_cast<float>(whitePoint -
                                blackLevelSeparate[2 + ((mOffset.x + 1) & 1)])))
         << 16;
  b = blackLevelSeparate[2 + (mOffset.x & 1)] |
      (blackLevelSeparate[2 + ((mOffset.x + 1) & 1)] << 16);

  for (int i = 0; i < 4; i++) {
    sub_mul[8 + i] = b;    // Subtract odd lines
    sub_mul[12 + i] = mul; // Multiply odd lines
  }

  sseround = _mm_set_epi32(512, 512, 512, 512);
  ssesub2 = _mm_set_epi32(32768, 32768, 32768, 32768);
  ssesign = _mm_set_epi32(0x80008000, 0x80008000, 0x80008000, 0x80008000);
  sse_full_scale_fp = _mm_set1_epi32(full_scale_fp | (full_scale_fp << 16));
  sse_half_scale_fp = _mm_set1_epi32(half_scale_fp >> 4);

  if (mDitherScale) {
    rand_mul = _mm_set1_epi32(0x4d9f1d32);
  } else {
    rand_mul = _mm_set1_epi32(0);
  }
  rand_mask = _mm_set1_epi32(0x00ff00ff); // 8 random bits

  for (int y = start_y; y < end_y; y++) {
    __m128i sserandom;
    if (mDitherScale) {
      sserandom =
          _mm_set_epi32(dim.x * 1676 + y * 18000, dim.x * 2342 + y * 34311,
                        dim.x * 4272 + y * 12123, dim.x * 1234 + y * 23464);
    } else {
      sserandom = _mm_setzero_si128();
    }
    auto* pixel = reinterpret_cast<__m128i*>(&data[(mOffset.y + y) * pitch]);
    __m128i ssescale, ssesub;
    if (((y + mOffset.y) & 1) == 0) {
      ssesub = _mm_load_si128(reinterpret_cast<__m128i*>(&sub_mul[0]));
      ssescale = _mm_load_si128(reinterpret_cast<__m128i*>(&sub_mul[4]));
    } else {
      ssesub = _mm_load_si128(reinterpret_cast<__m128i*>(&sub_mul[8]));
      ssescale = _mm_load_si128(reinterpret_cast<__m128i*>(&sub_mul[12]));
    }

    for (uint32 x = 0; x < gw; x++) {
      __m128i pix_high;
      __m128i temp;
      _mm_prefetch((char*)(pixel + 1), _MM_HINT_T0);
      __m128i pix_low = _mm_load_si128(pixel);
      // Subtract black
      pix_low = _mm_subs_epu16(pix_low, ssesub);
      // Multiply the two unsigned shorts and combine it to 32 bit result
      pix_high = _mm_mulhi_epu16(pix_low, ssescale);
      temp = _mm_mullo_epi16(pix_low, ssescale);
      pix_low = _mm_unpacklo_epi16(temp, pix_high);
      pix_high = _mm_unpackhi_epi16(temp, pix_high);
      // Add rounder
      pix_low = _mm_add_epi32(pix_low, sseround);
      pix_high = _mm_add_epi32(pix_high, sseround);

      sserandom = _mm_xor_si128(_mm_mulhi_epi16(sserandom, rand_mul),
                                _mm_mullo_epi16(sserandom, rand_mul));
      __m128i rand_masked =
          _mm_and_si128(sserandom, rand_mask); // Get 8 random bits
      rand_masked = _mm_mullo_epi16(rand_masked, sse_full_scale_fp);

      __m128i zero = _mm_setzero_si128();
      __m128i rand_lo = _mm_sub_epi32(sse_half_scale_fp,
                                      _mm_unpacklo_epi16(rand_masked, zero));
      __m128i rand_hi = _mm_sub_epi32(sse_half_scale_fp,
                                      _mm_unpackhi_epi16(rand_masked, zero));

      pix_low = _mm_add_epi32(pix_low, rand_lo);
      pix_high = _mm_add_epi32(pix_high, rand_hi);

      // Shift down
      pix_low = _mm_srai_epi32(pix_low, 10);
      pix_high = _mm_srai_epi32(pix_high, 10);
      // Subtract to avoid clipping
      pix_low = _mm_sub_epi32(pix_low, ssesub2);
      pix_high = _mm_sub_epi32(pix_high, ssesub2);
      // Pack
      pix_low = _mm_packs_epi32(pix_low, pix_high);
      // Shift sign off
      pix_low = _mm_xor_si128(pix_low, ssesign);
      _mm_store_si128(pixel, pix_low);
      pixel++;
    }
  }
  alignedFree(sub_mul);
}
#endif

void RawImageDataU16::scaleValues_plain(int start_y, int end_y) {
  int depth_values = whitePoint - blackLevelSeparate[0];
  float app_scale = 65535.0f / depth_values;

  // Scale in 30.2 fp
  auto full_scale_fp = static_cast<int>(app_scale * 4.0f);
  // Half Scale in 18.14 fp
  auto half_scale_fp = static_cast<int>(app_scale * 4095.0f);

  // Not SSE2
  int gw = dim.x * cpp;
  int mul[4];
  int sub[4];
  for (int i = 0; i < 4; i++) {
    int v = i;
    if ((mOffset.x & 1) != 0)
      v ^= 1;
    if ((mOffset.y & 1) != 0)
      v ^= 2;
    mul[i] = static_cast<int>(
        16384.0f * 65535.0f /
        static_cast<float>(whitePoint - blackLevelSeparate[v]));
    sub[i] = blackLevelSeparate[v];
  }
  for (int y = start_y; y < end_y; y++) {
    int v = dim.x + y * 36969;
    auto* pixel = reinterpret_cast<ushort16*>(getData(0, y));
    int* mul_local = &mul[2 * (y & 1)];
    int* sub_local = &sub[2 * (y & 1)];
    for (int x = 0; x < gw; x++) {
      int rand;
      if (mDitherScale) {
        v = 18000 * (v & 65535) + (v >> 16);
        rand = half_scale_fp - (full_scale_fp * (v & 2047));
      } else {
        rand = 0;
      }
      pixel[x] = clampBits(
          ((pixel[x] - sub_local[x & 1]) * mul_local[x & 1] + 8192 + rand) >>
              14,
          16);
    }
  }
}

/* This performs a 4 way interpolated pixel */
/* The value is interpolated from the 4 closest valid pixels in */
/* the horizontal and vertical direction. Pixels found further away */
/* are weighed less */

void RawImageDataU16::fixBadPixel( uint32 x, uint32 y, int component )
{
  array<int, 4> values;
  array<int, 4> dist;
  array<int, 4> weight;

  values.fill(-1);
  dist.fill(0);
  weight.fill(0);

  uchar8* bad_line = &mBadPixelMap[y*mBadPixelMapPitch];
  int step = isCFA ? 2 : 1;

  // Find pixel to the left
  int x_find = static_cast<int>(x) - step;
  int curr = 0;
  while (x_find >= 0 && values[curr] < 0) {
    if (0 == ((bad_line[x_find>>3] >> (x_find&7)) & 1)) {
      values[curr] =
          (reinterpret_cast<ushort16*>(getDataUncropped(x_find, y)))[component];
      dist[curr] = static_cast<int>(x) - x_find;
    }
    x_find -= step;
  }
  // Find pixel to the right
  x_find = static_cast<int>(x) + step;
  curr = 1;
  while (x_find < uncropped_dim.x && values[curr] < 0) {
    if (0 == ((bad_line[x_find>>3] >> (x_find&7)) & 1)) {
      values[curr] =
          (reinterpret_cast<ushort16*>(getDataUncropped(x_find, y)))[component];
      dist[curr] = x_find - static_cast<int>(x);
    }
    x_find += step;
  }

  bad_line = &mBadPixelMap[x>>3];
  // Find pixel upwards
  int y_find = static_cast<int>(y) - step;
  curr = 2;
  while (y_find >= 0 && values[curr] < 0) {
    if (0 == ((bad_line[y_find*mBadPixelMapPitch] >> (x&7)) & 1)) {
      values[curr] =
          (reinterpret_cast<ushort16*>(getDataUncropped(x, y_find)))[component];
      dist[curr] = static_cast<int>(y) - y_find;
    }
    y_find -= step;
  }
  // Find pixel downwards
  y_find = static_cast<int>(y) + step;
  curr = 3;
  while (y_find < uncropped_dim.y && values[curr] < 0) {
    if (0 == ((bad_line[y_find*mBadPixelMapPitch] >> (x&7)) & 1)) {
      values[curr] =
          (reinterpret_cast<ushort16*>(getDataUncropped(x, y_find)))[component];
      dist[curr] = y_find - static_cast<int>(y);
    }
    y_find += step;
  }

  // Find x weights
  int total_dist_x = dist[0] + dist[1];

  int total_shifts = 7;
  if (total_dist_x) {
    weight[0] = dist[0] ? (total_dist_x - dist[0]) * 256 / total_dist_x : 0;
    weight[1] = 256 - weight[0];
    total_shifts++;
  }

  // Find y weights
  int total_dist_y = dist[2] + dist[3];
  if (total_dist_y) {
    weight[2] = dist[2] ? (total_dist_y - dist[2]) * 256 / total_dist_y : 0;
    weight[3] = 256-weight[2];
    total_shifts++;
  }

  int total_pixel = 0;
  for (int i = 0; i < 4; i++)
    if (values[i] >= 0)
      total_pixel += values[i] * weight[i];

  total_pixel >>= total_shifts;
  auto* pix = reinterpret_cast<ushort16*>(getDataUncropped(x, y));
  pix[component] = clampBits(total_pixel, 16);

  /* Process other pixels - could be done inline, since we have the weights */
  if (cpp > 1 && component == 0)
    for (int i = 1; i < static_cast<int>(cpp); i++)
      fixBadPixel(x,y,i);
}

// TODO: Could be done with SSE2
void RawImageDataU16::doLookup( int start_y, int end_y )
{
  if (table->ntables == 1) {
    if (table->dither) {
      int gw = uncropped_dim.x * cpp;
      auto* t = reinterpret_cast<uint32*>(table->getTable(0));
      for (int y = start_y; y < end_y; y++) {
        uint32 v = (uncropped_dim.x + y * 13) ^ 0x45694584;
        auto* pixel = reinterpret_cast<ushort16*>(getDataUncropped(0, y));
        for (int x = 0 ; x < gw; x++) {
          ushort16 p = *pixel;
          uint32 lookup = t[p];
          uint32 base = lookup & 0xffff;
          uint32 delta = lookup >> 16;
          v = 15700 *(v & 65535) + (v >> 16);
          uint32 pix = base + (((delta * (v&2047) + 1024)) >> 12);
          *pixel = pix;
          pixel++;
        }
      }
      return;
    }

    int gw = uncropped_dim.x * cpp;
    ushort16 *t = table->getTable(0);
    for (int y = start_y; y < end_y; y++) {
      auto* pixel = reinterpret_cast<ushort16*>(getDataUncropped(0, y));
      for (int x = 0 ; x < gw; x++) {
        *pixel = t[*pixel];
        pixel ++;
      }
    }
    return;
  }
  ThrowRDE("Table lookup with multiple components not implemented");
}

} // namespace rawspeed
