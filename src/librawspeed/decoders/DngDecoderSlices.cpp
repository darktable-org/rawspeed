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

#include "decoders/DngDecoderSlices.h"
#include "common/Common.h"
#include "common/Memory.h" // for alignedMallocArray, alignedFree
#include "common/Point.h"
#include "decompressors/JpegDecompressor.h"
#include "decompressors/UncompressedDecompressor.h"
#include "io/IOException.h"
#include "tiff/TiffEntry.h"
#include "tiff/TiffIFD.h"
#include "tiff/TiffTag.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cfloat>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

extern "C" {
#ifdef HAVE_ZLIB
#include <zlib.h>
#endif
}

using namespace std;

namespace RawSpeed {

void *DecodeThread(void *_this) {
  auto *me = (DngDecoderThread *)_this;
  DngDecoderSlices* parent = me->parent;
  try {
    parent->decodeSlice(me);
  } catch (const std::exception &exc) {
    parent->mRaw->setError(
        string(string("DNGDEcodeThread: Caught exception: ") +
               string(exc.what()))
            .c_str());
  } catch (...) {
    parent->mRaw->setError("DNGDEcodeThread: Caught unhandled exception.");
  }
  return nullptr;
}

DngDecoderSlices::DngDecoderSlices(FileMap *file, const RawImage &img,
                                   int _compression)
    : mFile(file), mRaw(img) {
  mFixLjpeg = false;
  compression = _compression;
}

DngDecoderSlices::~DngDecoderSlices() = default;

void DngDecoderSlices::addSlice(const DngSliceElement &slice) {
  slices.push(slice);
}

void DngDecoderSlices::startDecoding() {
#ifdef NO_PTHREAD
  DngDecoderThread t;
  while (!slices.empty()) {
    t.slices.push(slices.front());
    slices.pop();
  }
  t.parent = this;
  DecodeThread(&t);
#else
  // Create threads

  nThreads = getThreadCount();
  int slicesPerThread = ((int)slices.size() + nThreads - 1) / nThreads;
//  decodedSlices = 0;
  pthread_attr_t attr;
  /* Initialize and set thread detached attribute */
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

  for (uint32 i = 0; i < nThreads; i++) {
    auto *t = new DngDecoderThread();
    for (int j = 0; j < slicesPerThread ; j++) {
      if (!slices.empty()) {
        t->slices.push(slices.front());
        slices.pop();
      }
    }
    t->parent = this;
    pthread_create(&t->threadid, &attr, DecodeThread, t);
    threads.push_back(t);
  }
  pthread_attr_destroy(&attr);

  void *status;
  for (uint32 i = 0; i < nThreads; i++) {
    pthread_join(threads[i]->threadid, &status);
    delete(threads[i]);
  }
#endif
}

#ifdef HAVE_ZLIB
// decodeFPDeltaRow(): MIT License, copyright 2014 Javier Celaya <jcelaya@gmail.com>
static inline void decodeFPDeltaRow(unsigned char *src, unsigned char *dst,
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

static inline uint32 fp16ToFloat(ushort16 fp16) {
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
  uint32 fp32_fraction = fp16_fraction << (23 - 10); // 23 is binary32 fraction size

  if (fp16_exponent == 31) {
    // Infinity or NaN
    fp32_exponent = 255;
  } else if (fp16_exponent == 0 && fp16_fraction == 0) {
    // +-Zero
    fp32_exponent = 0;
    fp32_fraction = 0;
  } else if (fp16_exponent == 0 && fp16_fraction != 0) {
    // Subnormal numbers
    // binary32 equation: -1 ^ sign * 2 ^ (exponent - 127) * 1.fraction
    // binary16 equation: -1 ^ sign * 2 ^ -14 * 0.fraction, we can represent it
    // as a normalized value in binary32, we have to shift fraction until we get
    // 1.new_fraction and decrement exponent for each shift
    fp32_exponent = -14 + 127;
    while (!(fp32_fraction & (1 << 23))) {
      fp32_exponent -= 1;
      fp32_fraction <<= 1;
    }
    fp32_fraction &= ((1 << 23) - 1);
  }
  return (sign << 31) | (fp32_exponent << 23) | fp32_fraction;
}

static inline uint32 fp24ToFloat(uint32 fp24) {
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
  uint32 fp32_fraction = fp24_fraction << (23 - 16); // 23 is binary 32 fraction size

  if (fp24_exponent == 127) {
    // Infinity or NaN
    fp32_exponent = 255;
  } else if (fp24_exponent == 0 && fp24_fraction == 0) {
    // +-Zero
    fp32_exponent = 0;
    fp32_fraction = 0;
  } else if (fp24_exponent == 0 && fp24_fraction != 0) {
    // Subnormal numbers
    // binary32 equation: -1 ^ sign * 2 ^ (exponent - 127) * 1.fraction
    // binary24 equation: -1 ^ sign * 2 ^ -62 * 0.fraction, we can represent it
    // as a normalized value in binary32, we have to shift fraction until we get
    // 1.new_fraction and decrement exponent for each shift
    fp32_exponent = -62 + 127;
    while (!(fp32_fraction & (1 << 23))) {
      fp32_exponent -= 1;
      fp32_fraction <<= 1;
    }
    fp32_fraction &= ((1 << 23) - 1);
  }
  return (sign << 31) | (fp32_exponent << 23) | fp32_fraction;
}

static inline void expandFP16(unsigned char *dst, int width) {
  auto *dst16 = (ushort16 *)dst;
  auto *dst32 = (uint32 *)dst;

  for (off_t x = width - 1; x >= 0; x--)
    dst32[x] = fp16ToFloat(dst16[x]);
}

static inline void expandFP24(unsigned char *dst, int width) {
  auto *dst32 = (uint32 *)dst;
  dst += (width - 1) * 3;
  for (off_t x = width - 1; x >= 0; x--) {
    dst32[x] = fp24ToFloat((dst[0] << 16) | (dst[1] << 8) | dst[2]);
    dst -= 3;
  }
}

void DngDecoderSlices::decodeDeflate(const DngSliceElement &e,
                                     unsigned char *uBuffer) {
  if (!mFile->isValid(e.byteOffset, e.byteCount))
    ThrowRDE("DngDecoderSlices::decodeDeflate: tile offset plus size is longer "
             "than file. Truncated file.");

  uLongf dstLen = e.width * e.height * 4UL;
  const unsigned char *cBuffer = mFile->getData(e.byteOffset, e.byteCount);
  int predFactor = 0;

  switch (mPredictor) {
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

  int err = uncompress(uBuffer, &dstLen, cBuffer, e.byteCount);
  if (err != Z_OK)
    ThrowRDE("DngDecoderSlices::decodeDeflate: failed to uncompress tile: %d",
             err);
  int bytesps = mBps / 8;
  size_t thisTileLength =
      e.offY + e.height > (uint32)mRaw->dim.y ? mRaw->dim.y - e.offY : e.height;
  size_t thisTileWidth =
      e.offX + e.width > (uint32)mRaw->dim.x ? mRaw->dim.x - e.offX : e.width;
  for (size_t row = 0; row < thisTileLength; ++row) {
    unsigned char *src = uBuffer + row * e.width * bytesps;
    unsigned char *dst = (unsigned char *)mRaw->getData() +
                         ((e.offY + row) * mRaw->pitch +
                          e.offX * sizeof(float) * mRaw->getCpp());
    if (predFactor)
      decodeFPDeltaRow(src, dst, thisTileWidth, e.width, bytesps, predFactor);
    switch (bytesps) {
    case 2:
      expandFP16(dst, thisTileWidth);
      break;
    case 3:
      expandFP24(dst, thisTileWidth);
      break;
    case 4:
    default:
      // No need to expand FP32
      break;
    }
  }
}
#endif

void DngDecoderSlices::decodeSlice(DngDecoderThread* t) {
  if (compression == 1) {
    while (!t->slices.empty()) {
      DngSliceElement e = t->slices.front();
      t->slices.pop();

      UncompressedDecompressor decompressor(*mFile, e.byteOffset, e.byteCount,
                                            mRaw,
                                            true /* does not matter here */);

      size_t thisTileLength = e.offY + e.height > (uint32)mRaw->dim.y
                                  ? mRaw->dim.y - e.offY
                                  : e.height;

      iPoint2D size(mRaw->dim.x, thisTileLength);
      iPoint2D pos(0, e.offY);

      bool big_endian = (getTiffEndianness(mFile) == big);
      // DNG spec says that if not 8 or 16 bit/sample, always use big endian
      if (mBps != 8 && mBps != 16)
        big_endian = true;

      try {
        decompressor.readUncompressedRaw(
            size, pos, mRaw->getCpp() * mRaw->dim.x * mBps / 8, mBps,
            big_endian ? BitOrder_Jpeg : BitOrder_Plain);
      } catch (RawDecoderException& err) {
        mRaw->setError(err.what());
      } catch (IOException& err) {
        mRaw->setError(err.what());
      }
    }
  } else if (compression == 7) {
    while (!t->slices.empty()) {
      DngSliceElement e = t->slices.front();
      t->slices.pop();
      LJpegPlain l(*mFile, e.byteOffset, e.byteCount, mRaw);
      l.mDNGCompatible = mFixLjpeg;
      try {
        l.decode(e.offX, e.offY);
      } catch (RawDecoderException &err) {
        mRaw->setError(err.what());
      } catch (IOException &err) {
        mRaw->setError(err.what());
      }
    }
    /* Deflate compression */
  } else if (compression == 8) {
#ifdef HAVE_ZLIB
    unsigned char *uBuffer = nullptr;
    while (!t->slices.empty()) {
      DngSliceElement e = t->slices.front();
      if (!uBuffer)
        uBuffer = new unsigned char [sizeof(float) * e.width * e.height];
      t->slices.pop();
      try {
        decodeDeflate(e, uBuffer);
      } catch (RawDecoderException &err) {
        mRaw->setError(err.what());
      } catch (IOException &err) {
        mRaw->setError(err.what());
      }
    }
    delete [] uBuffer;
#else
    ThrowRDE("DngDecoderSlices: deflate support is disabled.");
#endif
    /* Lossy DNG */
  } else if (compression == 0x884c) {
    /* Each slice is a JPEG image */
    while (!t->slices.empty()) {
      DngSliceElement e = t->slices.front();
      t->slices.pop();
      JpegDecompressor j(*mFile, e.byteOffset, e.byteCount, mRaw);
      try {
        j.decode(e.offX, e.offY);
      } catch (RawDecoderException &err) {
        mRaw->setError(err.what());
      } catch (IOException &err) {
        mRaw->setError(err.what());
      }
    }
  }
  else
    mRaw->setError("DngDecoderSlices: Unknown compression");
}

int DngDecoderSlices::size() {
  return (int)slices.size();
}

} // namespace RawSpeed
