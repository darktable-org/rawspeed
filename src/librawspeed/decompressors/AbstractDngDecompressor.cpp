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

#include "rawspeedconfig.h" // for HAVE_JPEG, HAVE_...
#include "decompressors/AbstractDngDecompressor.h"
#include "common/Common.h"                          // for BitOrder::BitOrd...
#include "common/Point.h"                           // for iPoint2D
#include "common/RawImage.h"                        // for RawImageData
#include "decoders/RawDecoderException.h"           // for RawDecoderException
#include "decompressors/DeflateDecompressor.h"      // for DeflateDecompressor
#include "decompressors/JpegDecompressor.h"         // for JpegDecompressor
#include "decompressors/LJpegDecompressor.h"        // for LJpegDecompressor
#include "decompressors/UncompressedDecompressor.h" // for UncompressedDeco...
#include "io/Buffer.h"                              // for Buffer (ptr only)
#include "io/ByteStream.h"                          // for ByteStream
#include "io/Endianness.h"                          // for Endianness, Endi...
#include "io/IOException.h"                         // for IOException
#include "tiff/TiffIFD.h"                           // for getTiffByteOrder
#include <cassert>                                  // for assert
#include <cstdio>                                   // for size_t
#include <limits>                                   // for numeric_limits
#include <memory>                                   // for unique_ptr
#include <vector>                                   // for vector

namespace rawspeed {

void AbstractDngDecompressor::decompress() const {
  startThreading(slices.size());
}

void AbstractDngDecompressor::decompressThreaded(
    const RawDecompressorThread* t) const {
  assert(t);
  assert(mRaw->dim.x > 0);
  assert(mRaw->dim.y > 0);
  assert(mRaw->getCpp() > 0 && mRaw->getCpp() <= 4);
  assert(mBps > 0 && mBps <= 32);

  if (compression == 1) {
    for (size_t i = t->start; i < t->end && i < slices.size(); i++) {
      auto e = &slices[i];

      UncompressedDecompressor decompressor(e->bs, mRaw);

      size_t thisTileLength =
          e->offY + e->height > static_cast<uint32>(mRaw->dim.y)
              ? mRaw->dim.y - e->offY
              : e->height;

      size_t thisTileWidth =
          e->offX + e->width > static_cast<uint32>(mRaw->dim.x)
              ? mRaw->dim.x - e->offX
              : e->width;

      if (thisTileLength == 0)
        ThrowRDE("Tile is empty. Can not decode!");

      iPoint2D tileSize(thisTileWidth, thisTileLength);
      iPoint2D pos(e->offX, e->offY);

      // FIXME: does bytestream have correct byteorder from the src file?
      bool big_endian = e->bs.getByteOrder() == Endianness::big;

      // DNG spec says that if not 8 or 16 bit/sample, always use big endian
      if (mBps != 8 && mBps != 16)
        big_endian = true;

      try {
        const uint32 inputPixelBits = mRaw->getCpp() * mBps;

        if (e->width > std::numeric_limits<int>::max() / inputPixelBits)
          ThrowIOE("Integer overflow when calculating input pitch");

        const int inputPitchBits = inputPixelBits * e->width;
        assert(inputPitchBits > 0);
        assert(inputPitchBits % 8 == 0);

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
    for (size_t i = t->start; i < t->end && i < slices.size(); i++) {
      auto e = &slices[i];
      LJpegDecompressor d(e->bs, mRaw);
      try {
        d.decode(e->offX, e->offY, e->width, e->height, mFixLjpeg);
      } catch (RawDecoderException& err) {
        mRaw->setError(err.what());
      } catch (IOException& err) {
        mRaw->setError(err.what());
      }
    }
    /* Deflate compression */
  } else if (compression == 8) {
#ifdef HAVE_ZLIB
    std::unique_ptr<unsigned char[]> uBuffer;
    for (size_t i = t->start; i < t->end && i < slices.size(); i++) {
      auto e = &slices[i];

      DeflateDecompressor z(e->bs, mRaw, mPredictor, mBps);
      try {
        z.decode(&uBuffer, e->width, e->height, e->offX, e->offY);
      } catch (RawDecoderException& err) {
        mRaw->setError(err.what());
      } catch (IOException& err) {
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
    for (size_t i = t->start; i < t->end && i < slices.size(); i++) {
      auto e = &slices[i];
      JpegDecompressor j(e->bs, mRaw);
      try {
        j.decode(e->offX, e->offY);
      } catch (RawDecoderException& err) {
        mRaw->setError(err.what());
      } catch (IOException& err) {
        mRaw->setError(err.what());
      }
    }
#else
#pragma message "JPEG is not present! Lossy JPEG DNG will not be supported!"
    ThrowRDE("jpeg support is disabled.");
#endif
  } else
    mRaw->setError("AbstractDngDecompressor: Unknown compression");
}

} // namespace rawspeed
