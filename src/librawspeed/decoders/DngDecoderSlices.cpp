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

#include "rawspeedconfig.h"
#include "decoders/DngDecoderSlices.h"
#include "common/Common.h"                          // for uint32, getThrea...
#include "common/Point.h"                           // for iPoint2D
#include "common/RawspeedException.h"               // for RawspeedException
#include "decoders/RawDecoderException.h"           // for RawDecoderException
#include "decompressors/DeflateDecompressor.h"      // for DeflateDecompressor
#include "decompressors/JpegDecompressor.h"         // for JpegDecompressor
#include "decompressors/LJpegDecompressor.h"        // for LJpegDecompressor
#include "decompressors/UncompressedDecompressor.h" // for UncompressedDeco...
#include "io/Endianness.h"                          // for Endianness::big
#include "io/IOException.h"                         // for IOException
#include "tiff/TiffIFD.h"                           // for getTiffEndianness
#include <algorithm>                                // for move
#include <cassert>                                  // for assert
#include <cstdio>                                   // for size_t
#include <memory>                                   // for allocator_traits...
#include <string>                                   // for string, operator+
#include <vector>                                   // for allocator, vector

using std::string;

namespace rawspeed {

void *DecodeThread(void *_this) {
  auto* me = static_cast<DngDecoderThread*>(_this);
  DngDecoderSlices* parent = me->parent;
  try {
    parent->decodeSlice(me);
  } catch (RawspeedException& e) {
    parent->mRaw->setError(string("Caught exception: ") + e.what());
  }
  return nullptr;
}

DngDecoderSlices::DngDecoderSlices(const Buffer* file, const RawImage& img,
                                   int _compression)
    : mFile(file), mRaw(img), mFixLjpeg(false), compression(_compression) {}

void DngDecoderSlices::addSlice(std::unique_ptr<DngSliceElement>&& slice) {
  slices.emplace(move(slice));
}

void DngDecoderSlices::startDecoding() {
#ifndef HAVE_PTHREAD
  DngDecoderThread t(this);
  while (!slices.empty()) {
    t.slices.emplace(move(slices.front()));
    slices.pop();
  }
  DecodeThread(&t);
#else
  // Create threads

  nThreads = getThreadCount();
  int slicesPerThread =
      (static_cast<int>(slices.size()) + nThreads - 1) / nThreads;
  //  decodedSlices = 0;
  pthread_attr_t attr;
  /* Initialize and set thread detached attribute */
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

  threads.reserve(nThreads);

  for (uint32 i = 0; i < nThreads; i++) {
    auto t = make_unique<DngDecoderThread>(this);
    for (int j = 0; j < slicesPerThread ; j++) {
      if (!slices.empty()) {
        t->slices.emplace(move(slices.front()));
        slices.pop();
      }
    }
    pthread_create(&t->threadid, &attr, DecodeThread, t.get());
    threads.emplace_back(move(t));
  }
  pthread_attr_destroy(&attr);

  void *status;
  for (auto& thread : threads) {
    pthread_join(thread->threadid, &status);
  }
  threads.clear();
#endif
}

void DngDecoderSlices::decodeSlice(DngDecoderThread* t) {
  assert(t);
  assert(mRaw->dim.x > 0);
  assert(mRaw->dim.y > 0);
  assert(mRaw->getCpp() > 0);
  assert(mBps > 0 && mBps <= 32);

  if (compression == 1) {
    while (!t->slices.empty()) {
      auto e = move(t->slices.front());
      t->slices.pop();

      UncompressedDecompressor decompressor(*mFile, e->byteOffset, e->byteCount,
                                            mRaw);

      size_t thisTileLength =
          e->offY + e->height > static_cast<uint32>(mRaw->dim.y)
              ? mRaw->dim.y - e->offY
              : e->height;

      if (thisTileLength == 0)
        ThrowRDE("Tile is empty. Can not decode!");

      iPoint2D tileSize(mRaw->dim.x, thisTileLength);
      iPoint2D pos(0, e->offY);

      const DataBuffer db(*mFile);
      const ByteStream bs(db);

      bool big_endian = (getTiffByteOrder(bs, 0) == Endianness::big);
      // DNG spec says that if not 8 or 16 bit/sample, always use big endian
      if (mBps != 8 && mBps != 16)
        big_endian = true;

      try {
        const int inputPitchBits = mRaw->getCpp() * mRaw->dim.x * mBps;
        assert(inputPitchBits > 0);

        const int inputPitch = inputPitchBits / 8;
        if (inputPitch == 0)
          ThrowRDE("Data input pitch is too short. Can not decode!");

        decompressor.readUncompressedRaw(tileSize, pos, inputPitch, mBps,
                                         big_endian ? BitOrder_MSB
                                                    : BitOrder_LSB);
      } catch (RawDecoderException& err) {
        mRaw->setError(err.what());
      } catch (IOException& err) {
        mRaw->setError(err.what());
      }
    }
  } else if (compression == 7) {
    while (!t->slices.empty()) {
      auto e = move(t->slices.front());
      t->slices.pop();
      LJpegDecompressor d(*mFile, e->byteOffset, e->byteCount, mRaw);
      try {
        d.decode(e->offX, e->offY, mFixLjpeg);
      } catch (RawDecoderException &err) {
        mRaw->setError(err.what());
      } catch (IOException &err) {
        mRaw->setError(err.what());
      }
    }
    /* Deflate compression */
  } else if (compression == 8) {
#ifdef HAVE_ZLIB
    std::unique_ptr<unsigned char[]> uBuffer;
    while (!t->slices.empty()) {
      auto e = move(t->slices.front());
      t->slices.pop();

      DeflateDecompressor z(*mFile, e->byteOffset, e->byteCount, mRaw,
                            mPredictor, mBps);
      try {
        z.decode(&uBuffer, e->width, e->height, e->offX, e->offY);
      } catch (RawDecoderException &err) {
        mRaw->setError(err.what());
      } catch (IOException &err) {
        mRaw->setError(err.what());
      }
    }
#else
#pragma message                                                                \
    "ZLIB is not present! Deflate compression will not be supported!"
    ThrowRDE("deflate support is disabled.");
#endif
    /* Lossy DNG */
  } else if (compression == 0x884c) {
#ifdef HAVE_JPEG
    /* Each slice is a JPEG image */
    while (!t->slices.empty()) {
      auto e = move(t->slices.front());
      t->slices.pop();
      JpegDecompressor j(*mFile, e->byteOffset, e->byteCount, mRaw);
      try {
        j.decode(e->offX, e->offY);
      } catch (RawDecoderException &err) {
        mRaw->setError(err.what());
      } catch (IOException &err) {
        mRaw->setError(err.what());
      }
    }
#else
#pragma message "JPEG is not present! Lossy JPEG DNG will not be supported!"
    ThrowRDE("jpeg support is disabled.");
#endif
  } else
    mRaw->setError("DngDecoderSlices: Unknown compression");
}

int __attribute__((pure)) DngDecoderSlices::size() {
  return static_cast<int>(slices.size());
}

} // namespace rawspeed
