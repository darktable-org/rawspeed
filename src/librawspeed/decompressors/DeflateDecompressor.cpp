/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2017 Vasily Khoruzhick

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

#include "rawspeedconfig.h"

#ifdef HAVE_ZLIB

#include "decompressors/DeflateDecompressor.h"
#include "common/Common.h"                // for uint32, ushort16
#include "common/Point.h"                 // for iPoint2D
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "io/Endianness.h"                // for getHostEndianness, Endiann...
#include <cassert>                        // for assert
#include <cstdio>                         // for size_t

extern "C" {
#include <zlib.h>
// IWYU pragma: no_include <zconf.h>
}

namespace rawspeed {

// decodeFPDeltaRow(): MIT License, copyright 2014 Javier Celaya
// <jcelaya@gmail.com>
static inline void decodeFPDeltaRow(unsigned char* src, unsigned char* dst,
                                    size_t tileWidth, size_t realTileWidth,
                                    unsigned int bytesps, int factor) {
  // DecodeDeltaBytes
  for (size_t col = factor; col < realTileWidth * bytesps; ++col) {
    src[col] += src[col - factor];
  }
  // Reorder bytes into the image
  // 16 and 32-bit versions depend on local architecture, 24-bit does not
  if (bytesps == 3) {
    for (size_t col = 0; col < tileWidth; ++col) {
      dst[col * 3] = src[col];
      dst[col * 3 + 1] = src[col + realTileWidth];
      dst[col * 3 + 2] = src[col + realTileWidth * 2];
    }
  } else {
    if (getHostEndianness() == little) {
      for (size_t col = 0; col < tileWidth; ++col) {
        for (size_t byte = 0; byte < bytesps; ++byte)
          dst[col * bytesps + byte] =
              src[col + realTileWidth * (bytesps - byte - 1)];
      }
    } else {
      for (size_t col = 0; col < tileWidth; ++col) {
        for (size_t byte = 0; byte < bytesps; ++byte)
          dst[col * bytesps + byte] = src[col + realTileWidth * byte];
      }
    }
  }
}

static inline uint32 __attribute__((const)) fp16ToFloat(ushort16 fp16) {
  // IEEE-754-2008: binary16:
  // bit 15 - sign
  // bits 14-10 - exponent (5 bit)
  // bits 9-0 - fraction (10 bit)
  //
  // exp = 0, fract = +-0: zero
  // exp = 0; fract != 0: subnormal numbers
  //                      equation: -1 ^ sign * 2 ^ -14 * 0.fraction
  // exp = 1..30: normalized value
  //              equation: -1 ^ sign * 2 ^ (exponent - 15) * 1.fraction
  // exp = 31, fract = +-0: +-infinity
  // exp = 31, fract != 0: NaN

  uint32 sign = (fp16 >> 15) & 1;
  uint32 fp16_exponent = (fp16 >> 10) & ((1 << 5) - 1);
  uint32 fp16_fraction = fp16 & ((1 << 10) - 1);

  // Normalized or zero
  // binary32 equation: -1 ^ sign * 2 ^ (exponent - 127) * 1.fraction
  // => exponent32 - 127 = exponent16 - 15, exponent32 = exponent16 + 127 - 15
  uint32 fp32_exponent = fp16_exponent + 127 - 15;
  uint32 fp32_fraction = fp16_fraction
                         << (23 - 10); // 23 is binary32 fraction size

  if (fp16_exponent == 31) {
    // Infinity or NaN
    fp32_exponent = 255;
  } else if (fp16_exponent == 0) {
    if (fp16_fraction == 0) {
      // +-Zero
      fp32_exponent = 0;
      fp32_fraction = 0;
    } else {
      // Subnormal numbers
      // binary32 equation: -1 ^ sign * 2 ^ (exponent - 127) * 1.fraction
      // binary16 equation: -1 ^ sign * 2 ^ -14 * 0.fraction, we can represent
      // it as a normalized value in binary32, we have to shift fraction until
      // we get 1.new_fraction and decrement exponent for each shift
      fp32_exponent = -14 + 127;
      while (!(fp32_fraction & (1 << 23))) {
        fp32_exponent -= 1;
        fp32_fraction <<= 1;
      }
      fp32_fraction &= ((1 << 23) - 1);
    }
  }
  return (sign << 31) | (fp32_exponent << 23) | fp32_fraction;
}

static inline uint32 __attribute__((const)) fp24ToFloat(uint32 fp24) {
  // binary24: Not a part of IEEE754-2008, but format is obvious,
  // see https://en.wikipedia.org/wiki/Minifloat
  // bit 23 - sign
  // bits 22-16 - exponent (7 bit)
  // bits 15-0 - fraction (16 bit)
  //
  // exp = 0, fract = +-0: zero
  // exp = 0; fract != 0: subnormal numbers
  //                      equation: -1 ^ sign * 2 ^ -62 * 0.fraction
  // exp = 1..126: normalized value
  //              equation: -1 ^ sign * 2 ^ (exponent - 63) * 1.fraction
  // exp = 127, fract = +-0: +-infinity
  // exp = 127, fract != 0: NaN

  uint32 sign = (fp24 >> 23) & 1;
  uint32 fp24_exponent = (fp24 >> 16) & ((1 << 7) - 1);
  uint32 fp24_fraction = fp24 & ((1 << 16) - 1);

  // Normalized or zero
  // binary32 equation: -1 ^ sign * 2 ^ (exponent - 127) * 1.fraction
  // => exponent32 - 127 = exponent24 - 64, exponent32 = exponent16 + 127 - 63
  uint32 fp32_exponent = fp24_exponent + 127 - 63;
  uint32 fp32_fraction = fp24_fraction
                         << (23 - 16); // 23 is binary 32 fraction size

  if (fp24_exponent == 127) {
    // Infinity or NaN
    fp32_exponent = 255;
  } else if (fp24_exponent == 0) {
    if (fp24_fraction == 0) {
      // +-Zero
      fp32_exponent = 0;
      fp32_fraction = 0;
    } else {
      // Subnormal numbers
      // binary32 equation: -1 ^ sign * 2 ^ (exponent - 127) * 1.fraction
      // binary24 equation: -1 ^ sign * 2 ^ -62 * 0.fraction, we can represent
      // it as a normalized value in binary32, we have to shift fraction until
      // we get 1.new_fraction and decrement exponent for each shift
      fp32_exponent = -62 + 127;
      while (!(fp32_fraction & (1 << 23))) {
        fp32_exponent -= 1;
        fp32_fraction <<= 1;
      }
      fp32_fraction &= ((1 << 23) - 1);
    }
  }
  return (sign << 31) | (fp32_exponent << 23) | fp32_fraction;
}

static inline void expandFP16(unsigned char* dst, int width) {
  auto* dst16 = reinterpret_cast<ushort16*>(dst);
  auto* dst32 = reinterpret_cast<uint32*>(dst);

  for (int x = width - 1; x >= 0; x--)
    dst32[x] = fp16ToFloat(dst16[x]);
}

static inline void expandFP24(unsigned char* dst, int width) {
  auto* dst32 = reinterpret_cast<uint32*>(dst);
  dst += (width - 1) * 3;
  for (int x = width - 1; x >= 0; x--) {
    dst32[x] = fp24ToFloat((dst[0] << 16) | (dst[1] << 8) | dst[2]);
    dst -= 3;
  }
}

void DeflateDecompressor::decode(unsigned char** uBuffer, int width, int height,
                                 uint32 offX, uint32 offY) {
  uLongf dstLen = sizeof(float) * width * height;

  if (!*uBuffer)
    *uBuffer = new unsigned char[dstLen];

  const auto cSize = input.getRemainSize();
  const unsigned char* cBuffer = input.getData(cSize);

  int err = uncompress(*uBuffer, &dstLen, cBuffer, cSize);
  if (err != Z_OK) {
    ThrowRDE("failed to uncompress tile: %d (%s)", err, zError(err));
  }

  int predFactor = 0;
  switch (predictor) {
  case 3:
    predFactor = 1;
    break;
  case 34894:
    predFactor = 2;
    break;
  case 34895:
    predFactor = 4;
    break;
  default:
    predFactor = 0;
    break;
  }

  int bytesps = bps / 8;
  size_t thisTileLength = offY + height > static_cast<uint32>(mRaw->dim.y)
                              ? mRaw->dim.y - offY
                              : height;
  size_t thisTileWidth = offX + width > static_cast<uint32>(mRaw->dim.x)
                             ? mRaw->dim.x - offX
                             : width;

  for (size_t row = 0; row < thisTileLength; ++row) {
    unsigned char* src = *uBuffer + row * width * bytesps;
    unsigned char* dst =
        static_cast<unsigned char*>(mRaw->getData()) +
        ((offY + row) * mRaw->pitch + offX * sizeof(float) * mRaw->getCpp());

    if (predFactor)
      decodeFPDeltaRow(src, dst, thisTileWidth, width, bytesps, predFactor);

    assert(bytesps >= 2 && bytesps <= 4);
    switch (bytesps) {
    case 2:
      expandFP16(dst, thisTileWidth);
      break;
    case 3:
      expandFP24(dst, thisTileWidth);
      break;
    case 4:
      // No need to expand FP32
      break;
    default:
      __builtin_unreachable();
      break;
    }
  }
}

} // namespace rawspeed

#else

#pragma message                                                                \
    "ZLIB is not present! Deflate compression will not be supported!"

#endif
