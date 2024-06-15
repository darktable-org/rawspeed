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

#include "rawspeedconfig.h"
#include "decompressors/AbstractDngDecompressor.h"
#include "adt/Array1DRef.h"
#include "adt/Casts.h"
#include "adt/Invariant.h"
#include "adt/Point.h"
#include "bitstreams/BitStreams.h"
#include "common/Common.h"
#include "common/RawImage.h"
#include "decoders/RawDecoderException.h"
#include "decompressors/LJpegDecoder.h"
#include "decompressors/UncompressedDecompressor.h"
#include "decompressors/VC5Decompressor.h"
#include "io/ByteStream.h"
#include "io/Endianness.h"
#include "io/IOException.h"
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#ifdef HAVE_ZLIB
#include "decompressors/DeflateDecompressor.h"
#include <memory>
#endif

#ifdef HAVE_JPEG
#include "decompressors/JpegDecompressor.h"
#endif

namespace rawspeed {

template <> void AbstractDngDecompressor::decompressThread<1>() const noexcept {
#ifdef HAVE_OPENMP
#pragma omp for schedule(static)
#endif
  for (const auto& e :
       Array1DRef(slices.data(), implicit_cast<int>(slices.size()))) {
    try {
      iPoint2D tileSize(e.width, e.height);
      iPoint2D pos(e.offX, e.offY);

      bool big_endian = e.bs.getByteOrder() == Endianness::big;

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

      const uint32_t inputPixelBits = mRaw->getCpp() * mBps;

      if (e.dsc.tileW > std::numeric_limits<int>::max() / inputPixelBits)
        ThrowIOE("Integer overflow when calculating input pitch");

      const int inputPitchBits = inputPixelBits * e.dsc.tileW;
      invariant(inputPitchBits > 0);

      if (inputPitchBits % 8 != 0) {
        ThrowRDE("Bad combination of cpp (%u), bps (%u) and width (%u), the "
                 "pitch is %u bits, which is not a multiple of 8 (1 byte)",
                 mRaw->getCpp(), mBps, e.width, inputPitchBits);
      }

      const int inputPitch = inputPitchBits / 8;
      if (inputPitch == 0)
        ThrowRDE("Data input pitch is too short. Can not decode!");

      UncompressedDecompressor decompressor(
          e.bs, mRaw, iRectangle2D(pos, tileSize), inputPitch, mBps,
          big_endian ? BitOrder::MSB : BitOrder::LSB);
      decompressor.readUncompressedRaw();
    } catch (const RawDecoderException& err) {
      mRaw->setError(err.what());
    } catch (const IOException& err) {
      mRaw->setError(err.what());
    } catch (...) {
      // We should not get any other exception type here.
      __builtin_unreachable();
    }
  }
}

template <> void AbstractDngDecompressor::decompressThread<7>() const noexcept {
#ifdef HAVE_OPENMP
#pragma omp for schedule(static)
#endif
  for (const auto& e :
       Array1DRef(slices.data(), implicit_cast<int>(slices.size()))) {
    try {
      LJpegDecoder d(e.bs, mRaw);
      d.decode(e.offX, e.offY, e.width, e.height,
               iPoint2D(e.dsc.tileW, e.dsc.tileH), mFixLjpeg);
    } catch (const RawDecoderException& err) {
      mRaw->setError(err.what());
    } catch (const IOException& err) {
      mRaw->setError(err.what());
    } catch (...) {
      // We should not get any other exception type here.
      __builtin_unreachable();
    }
  }
}

#ifdef HAVE_ZLIB
template <> void AbstractDngDecompressor::decompressThread<8>() const noexcept {
  // NOLINTNEXTLINE(modernize-avoid-c-arrays)
  std::unique_ptr<unsigned char[]> uBuffer;

#ifdef HAVE_OPENMP
#pragma omp for schedule(static)
#endif
  for (const auto& e :
       Array1DRef(slices.data(), implicit_cast<int>(slices.size()))) {
    try {
      DeflateDecompressor z(e.bs.peekBuffer(e.bs.getRemainSize()), mRaw,
                            mPredictor, mBps);
      z.decode(&uBuffer, iPoint2D(mRaw->getCpp() * e.dsc.tileW, e.dsc.tileH),
               iPoint2D(mRaw->getCpp() * e.width, e.height),
               iPoint2D(mRaw->getCpp() * e.offX, e.offY));
    } catch (const RawDecoderException& err) {
      mRaw->setError(err.what());
    } catch (const IOException& err) {
      mRaw->setError(err.what());
    } catch (...) {
      // We should not get any other exception type here.
      __builtin_unreachable();
    }
  }
}
#endif

template <> void AbstractDngDecompressor::decompressThread<9>() const noexcept {
#ifdef HAVE_OPENMP
#pragma omp for schedule(static)
#endif
  for (const auto& e :
       Array1DRef(slices.data(), implicit_cast<int>(slices.size()))) {
    try {
      VC5Decompressor d(e.bs, mRaw);
      d.decode(e.offX, e.offY, e.width, e.height);
    } catch (const RawDecoderException& err) {
      mRaw->setError(err.what());
    } catch (const IOException& err) {
      mRaw->setError(err.what());
    } catch (...) {
      // We should not get any other exception type here.
      __builtin_unreachable();
    }
  }
}

#ifdef HAVE_JPEG
template <>
void AbstractDngDecompressor::decompressThread<0x884c>() const noexcept {
#ifdef HAVE_OPENMP
#pragma omp for schedule(static)
#endif
  for (const auto& e :
       Array1DRef(slices.data(), implicit_cast<int>(slices.size()))) {
    try {
      JpegDecompressor j(e.bs.peekBuffer(e.bs.getRemainSize()), mRaw);
      j.decode(e.offX, e.offY);
    } catch (const RawDecoderException& err) {
      mRaw->setError(err.what());
    } catch (const IOException& err) {
      mRaw->setError(err.what());
    } catch (...) {
      // We should not get any other exception type here.
      __builtin_unreachable();
    }
  }
}
#endif

void AbstractDngDecompressor::decompressThread() const noexcept {
  invariant(mRaw->dim.x > 0);
  invariant(mRaw->dim.y > 0);
  invariant(mRaw->getCpp() > 0 && mRaw->getCpp() <= 4);
  invariant(mBps > 0 && mBps <= 32);

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
