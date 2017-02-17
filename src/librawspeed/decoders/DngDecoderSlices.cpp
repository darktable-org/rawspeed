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
#include "decoders/RawDecoderException.h"           // for RawDecoderException
#include "decompressors/DeflateDecompressor.h"      // for DeflateDecompressor
#include "decompressors/JpegDecompressor.h"         // for JpegDecompressor
#include "decompressors/LJpegDecompressor.h"        // for LJpegDecompressor
#include "decompressors/UncompressedDecompressor.h" // for UncompressedDeco...
#include "io/Endianness.h"                          // for Endianness::big
#include "io/IOException.h"                         // for IOException
#include "tiff/TiffEntry.h"                         // IWYU pragma: keep
#include "tiff/TiffIFD.h"                           // for getTiffEndianness
#include <cstdio>                                   // for size_t
#include <exception>                                // for exception
#include <string>                                   // for string, operator+
#include <vector>                                   // for allocator, vector

using namespace std;

namespace RawSpeed {

void *DecodeThread(void *_this) {
  auto *me = (DngDecoderThread *)_this;
  DngDecoderSlices* parent = me->parent;
  try {
    parent->decodeSlice(me);
  } catch (const std::exception &exc) {
    parent->mRaw->setError(string(
        string("DNGDEcodeThread: Caught exception: ") + string(exc.what())));
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
#ifndef HAVE_PTHREAD
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
      LJpegDecompressor d(*mFile, e.byteOffset, e.byteCount, mRaw);
      try {
        d.decode(e.offX, e.offY, mFixLjpeg);
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
      t->slices.pop();

      DeflateDecompressor z(*mFile, e.byteOffset, e.byteCount, mRaw, mPredictor,
                            mBps);
      try {
        z.decode(&uBuffer, e.width, e.height, e.offX, e.offY);
      } catch (RawDecoderException &err) {
        mRaw->setError(err.what());
      } catch (IOException &err) {
        mRaw->setError(err.what());
      }
    }
    delete [] uBuffer;
#else
#pragma message                                                                \
    "ZLIB is not present! Deflate compression will not be supported!"
    ThrowRDE("DngDecoderSlices: deflate support is disabled.");
#endif
    /* Lossy DNG */
  } else if (compression == 0x884c) {
#ifdef HAVE_JPEG
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
#else
#pragma message "JPEG is not present! Lossy JPEG DNG will not be supported!"
    ThrowRDE("DngDecoderSlices: jpeg support is disabled.");
#endif
  }
  else
    mRaw->setError("DngDecoderSlices: Unknown compression");
}

int DngDecoderSlices::size() {
  return (int)slices.size();
}

} // namespace RawSpeed
