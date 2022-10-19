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

#include "common/FloatingPoint.h"         // for fp16ToFloat, fp24ToFloat
#include "common/Point.h"                 // for iPoint2D
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "decompressors/DeflateDecompressor.h"
#include "io/Endianness.h" // for getHostEndianness, Endianness
#include <cassert>         // for assert
#include <cstdint>         // for uint32_t, uint16_t
#include <cstdio>          // for size_t
#include <zlib.h>          // for uncompress, zError, Z_OK

namespace rawspeed {

// decodeFPDeltaRow(): MIT License, copyright 2014 Javier Celaya
// <jcelaya@gmail.com>
static inline void decodeDeltaBytes(unsigned char* src, size_t realTileWidth,
                                    unsigned int bytesps, int factor) {
  for (size_t col = factor; col < realTileWidth * bytesps; ++col) {
    // Yes, this is correct, and is symmetrical with EncodeDeltaBytes in
    // hdrmerge, and they both combined are lossless.
    // This is indeed working in modulo-2^n arighmetics.
    src[col] = static_cast<unsigned char>(src[col] + src[col - factor]);
  }
}

// decodeFPDeltaRow(): MIT License, copyright 2014 Javier Celaya
// <jcelaya@gmail.com>
static inline void decodeFPDeltaRow(unsigned char* src, unsigned char* dst,
                                    size_t tileWidth, size_t realTileWidth,
                                    unsigned int bytesps) {
  // Reorder bytes into the image
  // 16 and 32-bit versions depend on local architecture, 24-bit does not
  if (bytesps == 3) {
    for (size_t col = 0; col < tileWidth; ++col) {
      dst[col * 3] = src[col];
      dst[col * 3 + 1] = src[col + realTileWidth];
      dst[col * 3 + 2] = src[col + realTileWidth * 2];
    }
  } else {
    if (getHostEndianness() == Endianness::little) {
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

static inline void expandFP16(unsigned char* src, unsigned char* dst,
                              int width) {
  const auto* src16 = reinterpret_cast<uint16_t*>(src);
  auto* dst32 = reinterpret_cast<uint32_t*>(dst);
  for (int x = 0; x < width; x++)
    dst32[x] = fp16ToFloat(src16[x]);
}

static inline void expandFP24(unsigned char* src, unsigned char* dst,
                              int width) {
  const auto* src8 = reinterpret_cast<uint8_t*>(src);
  auto* dst32 = reinterpret_cast<uint32_t*>(dst);
  for (int x = 0; x < width; x++)
    dst32[x] = fp24ToFloat((src8[3 * x + 0] << 16) | (src8[3 * x + 1] << 8) |
                           src8[3 * x + 2]);
}

void DeflateDecompressor::decode(
    std::unique_ptr<unsigned char[]>* uBuffer, // NOLINT
    iPoint2D maxDim, iPoint2D dim, iPoint2D off) {
  uLongf dstLen = sizeof(float) * maxDim.area();

  if (!*uBuffer)
    *uBuffer =
        std::unique_ptr<unsigned char[]>(new unsigned char[dstLen]); // NOLINT

  const auto cSize = input.getRemainSize();
  const unsigned char* cBuffer = input.getData(cSize);

  if (int err = uncompress(uBuffer->get(), &dstLen, cBuffer, cSize);
      err != Z_OK) {
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
  predFactor *= mRaw->getCpp();

  int bytesps = bps / 8;

  std::vector<unsigned char> tmp_storage;
  if (predFactor && bytesps != 4)
    tmp_storage.resize(bytesps * dim.x);

  for (auto row = 0; row < dim.y; ++row) {
    unsigned char* src = uBuffer->get() + row * maxDim.x * bytesps;
    unsigned char* dst =
        mRaw->getData() + ((off.y + row) * mRaw->pitch + off.x * sizeof(float));
    unsigned char* tmp = dst;

    if (predFactor) {
      if (bytesps != 4)
        tmp = tmp_storage.data();
      decodeDeltaBytes(src, maxDim.x, bytesps, predFactor);
      decodeFPDeltaRow(src, tmp, dim.x, maxDim.x, bytesps);
    }

    assert(bytesps >= 2 && bytesps <= 4);
    switch (bytesps) {
    case 2:
      expandFP16(tmp, dst, dim.x);
      break;
    case 3:
      expandFP24(tmp, dst, dim.x);
      break;
    case 4:
      // No need to expand FP32
      break;
    default:
      __builtin_unreachable();
    }
  }
}

} // namespace rawspeed

#else

#pragma message                                                                \
    "ZLIB is not present! Deflate compression will not be supported!"

#endif
