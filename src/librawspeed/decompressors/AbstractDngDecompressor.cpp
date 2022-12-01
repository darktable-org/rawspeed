/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2017-2018 Roman Lebeedv

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

#include "rawspeedconfig.h" // for HAVE_OPENMP, HAV...
#include "decompressors/AbstractDngDecompressor.h"
#include "adt/Point.h"                              // for iPoint2D
#include "common/Common.h"                          // for rawspeed_get_num...
#include "common/RawImage.h"                        // for RawImageData
#include "decoders/RawDecoderException.h"           // for RawDecoderException
#include "decompressors/DeflateDecompressor.h"      // for DeflateDecompressor
#include "decompressors/JpegDecompressor.h"         // for JpegDecompressor
#include "decompressors/LJpegDecompressor.h"        // for LJpegDecompressor
#include "decompressors/UncompressedDecompressor.h" // for UncompressedDeco...
#include "decompressors/VC5Decompressor.h"          // for VC5Decompressor
#include "io/ByteStream.h"                          // for ByteStream
#include "io/Endianness.h"                          // for Endianness, Endi...
#include "io/IOException.h"                         // for IOException, Thr...
#include <cassert>                                  // for assert
#include <limits>                                   // for numeric_limits
#include <memory>                                   // for unique_ptr
#include <string>                                   // for string
#include <vector>                                   // for vector, allocator

namespace rawspeed {

template <> void AbstractDngDecompressor::decompressThread<1>() const noexcept {
#ifdef HAVE_OPENMP
#pragma omp for schedule(static)
#endif
  for (auto e = slices.cbegin(); e < slices.cend(); ++e) {
    UncompressedDecompressor decompressor(e->bs, mRaw);

    iPoint2D tileSize(e->width, e->height);
    iPoint2D pos(e->offX, e->offY);

    bool big_endian = e->bs.getByteOrder() == Endianness::big;

    // DNG spec says that if not 8/16/32 bit/sample, always use big endian.
    // It's not very obvious, but that does not appear to apply to FP.
    switch (mBps) {
    case 8:
    case 16:
    case 32:
      break;
    default:
      if (mRaw->getDataType() == RawImageType::UINT16)
        big_endian = true;
      break;
    }

    try {
      const uint32_t inputPixelBits = mRaw->getCpp() * mBps;

      if (e->dsc.tileW > std::numeric_limits<int>::max() / inputPixelBits)
        ThrowIOE("Integer overflow when calculating input pitch");

      const int inputPitchBits = inputPixelBits * e->dsc.tileW;
      assert(inputPitchBits > 0);

      if (inputPitchBits % 8 != 0) {
        ThrowRDE("Bad combination of cpp (%u), bps (%u) and width (%u), the "
                 "pitch is %u bits, which is not a multiple of 8 (1 byte)",
                 mRaw->getCpp(), mBps, e->width, inputPitchBits);
      }

      const int inputPitch = inputPitchBits / 8;
      if (inputPitch == 0)
        ThrowRDE("Data input pitch is too short. Can not decode!");

      decompressor.readUncompressedRaw(tileSize, pos, inputPitch, mBps,
                                       big_endian ? BitOrder::MSB
                                                  : BitOrder::LSB);
    } catch (const RawDecoderException& err) {
      mRaw->setError(err.what());
    } catch (const IOException& err) {
      mRaw->setError(err.what());
    }
  }
}

template <> void AbstractDngDecompressor::decompressThread<7>() const noexcept {
#ifdef HAVE_OPENMP
#pragma omp for schedule(static)
#endif
  for (auto e = slices.cbegin(); e < slices.cend(); ++e) {
    try {
      LJpegDecompressor d(e->bs, mRaw);
      d.decode(e->offX, e->offY, e->width, e->height, mFixLjpeg);
    } catch (const RawDecoderException& err) {
      mRaw->setError(err.what());
    } catch (const IOException& err) {
      mRaw->setError(err.what());
    }
  }
}

#ifdef HAVE_ZLIB
template <> void AbstractDngDecompressor::decompressThread<8>() const noexcept {
  std::unique_ptr<unsigned char[]> uBuffer; // NOLINT

#ifdef HAVE_OPENMP
#pragma omp for schedule(static)
#endif
  for (auto e = slices.cbegin(); e < slices.cend(); ++e) {
    DeflateDecompressor z(e->bs, mRaw, mPredictor, mBps);
    try {
      z.decode(&uBuffer, iPoint2D(mRaw->getCpp() * e->dsc.tileW, e->dsc.tileH),
               iPoint2D(mRaw->getCpp() * e->width, e->height),
               iPoint2D(mRaw->getCpp() * e->offX, e->offY));
    } catch (const RawDecoderException& err) {
      mRaw->setError(err.what());
    } catch (const IOException& err) {
      mRaw->setError(err.what());
    }
  }
}
#endif

template <> void AbstractDngDecompressor::decompressThread<9>() const noexcept {
#ifdef HAVE_OPENMP
#pragma omp for schedule(static)
#endif
  for (auto e = slices.cbegin(); e < slices.cend(); ++e) {
    try {
      VC5Decompressor d(e->bs, mRaw);
      d.decode(e->offX, e->offY, e->width, e->height);
    } catch (const RawDecoderException& err) {
      mRaw->setError(err.what());
    } catch (const IOException& err) {
      mRaw->setError(err.what());
    }
  }
}

#ifdef HAVE_JPEG
template <>
void AbstractDngDecompressor::decompressThread<0x884c>() const noexcept {
#ifdef HAVE_OPENMP
#pragma omp for schedule(static)
#endif
  for (auto e = slices.cbegin(); e < slices.cend(); ++e) {
    JpegDecompressor j(e->bs, mRaw);
    try {
      j.decode(e->offX, e->offY);
    } catch (const RawDecoderException& err) {
      mRaw->setError(err.what());
    } catch (const IOException& err) {
      mRaw->setError(err.what());
    }
  }
}
#endif

void AbstractDngDecompressor::decompressThread() const noexcept {
  assert(mRaw->dim.x > 0);
  assert(mRaw->dim.y > 0);
  assert(mRaw->getCpp() > 0 && mRaw->getCpp() <= 4);
  assert(mBps > 0 && mBps <= 32);

  if (compression == 1) {
    /* Uncompressed */
    decompressThread<1>();
  } else if (compression == 7) {
    /* Lossless JPEG */
    decompressThread<7>();
  } else if (compression == 8) {
    /* Deflate compression */
#ifdef HAVE_ZLIB
    decompressThread<8>();
#else
#pragma message                                                                \
    "ZLIB is not present! Deflate compression will not be supported!"
    mRaw->setError("deflate support is disabled.");
#endif
  } else if (compression == 9) {
    /* GOPRO VC-5 */
    decompressThread<9>();
  } else if (compression == 0x884c) {
    /* Lossy DNG */
#ifdef HAVE_JPEG
    decompressThread<0x884c>();
#else
#pragma message "JPEG is not present! Lossy JPEG DNG will not be supported!"
    mRaw->setError("jpeg support is disabled.");
#endif
  } else
    mRaw->setError("AbstractDngDecompressor: Unknown compression");
}

void AbstractDngDecompressor::decompress() const {
#ifdef HAVE_OPENMP
#pragma omp parallel default(none) num_threads(                                \
    rawspeed_get_number_of_processor_cores()) if (slices.size() > 1)
#endif
  decompressThread();

  std::string firstErr;
  if (mRaw->isTooManyErrors(1, &firstErr)) {
    ThrowRDE("Too many errors encountered. Giving up. First Error:\n%s",
             firstErr.c_str());
  }
}

} // namespace rawspeed
