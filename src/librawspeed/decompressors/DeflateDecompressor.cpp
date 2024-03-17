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

#include "rawspeedconfig.h" // IWYU pragma: keep
#include <bit>

#ifdef HAVE_ZLIB

#include "adt/Array1DRef.h"
#include "adt/Array2DRef.h"
#include "adt/Casts.h"
#include "adt/CroppedArray1DRef.h"
#include "adt/CroppedArray2DRef.h"
#include "adt/Invariant.h"
#include "adt/Point.h"
#include "common/FloatingPoint.h"
#include "common/RawImage.h"
#include "decoders/RawDecoderException.h"
#include "decompressors/DeflateDecompressor.h"
#include "io/Buffer.h"
#include "io/Endianness.h"
#include <array>
#include <climits>
#include <cstdint>
#include <memory>
#include <utility>
#include <zconf.h>
#include <zlib.h>

namespace rawspeed {

DeflateDecompressor::DeflateDecompressor(Buffer bs, RawImage img, int predictor,
                                         int bps_)
    : input(bs), mRaw(std::move(img)), bps(bps_) {
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
    ThrowRDE("Unsupported predictor %i", predictor);
  }
  predFactor *= mRaw->getCpp();
}

namespace {

// decodeFPDeltaRow(): MIT License, copyright 2014 Javier Celaya
// <jcelaya@gmail.com>
inline void decodeDeltaBytes(Array1DRef<unsigned char> src, int realTileWidth,
                             int bytesps, int factor) {
  for (int col = factor; col < realTileWidth * bytesps; ++col) {
    // Yes, this is correct, and is symmetrical with EncodeDeltaBytes in
    // hdrmerge, and they both combined are lossless.
    // This is indeed working in modulo-2^n arighmetics.
    src(col) = static_cast<unsigned char>(src(col) + src(col - factor));
  }
}

} // namespace

template <typename T> struct StorageType final {};
template <> struct StorageType<ieee_754_2008::Binary16> {
  using type = uint16_t;
  static constexpr int padding_bytes = 0;
};
template <> struct StorageType<ieee_754_2008::Binary24> {
  using type = uint32_t;
  static constexpr int padding_bytes = 1;
};
template <> struct StorageType<ieee_754_2008::Binary32> {
  using type = uint32_t;
  static constexpr int padding_bytes = 0;
};

namespace {

template <typename T>
inline void decodeFPDeltaRow(Array1DRef<const unsigned char> src,
                             int realTileWidth, CroppedArray1DRef<float> out) {
  using storage_type = typename StorageType<T>::type;
  constexpr unsigned storage_bytes = sizeof(storage_type);
  constexpr unsigned bytesps = T::StorageWidth / 8;

  for (int col = 0; col < out.size(); ++col) {
    std::array<unsigned char, storage_bytes> bytes;
    for (int c = 0; c != bytesps; ++c)
      bytes[c] = src(col + c * realTileWidth);

    auto tmp = getBE<storage_type>(bytes.data());
    tmp >>= CHAR_BIT * StorageType<T>::padding_bytes;

    uint32_t tmp_expanded;
    switch (bytesps) {
    case 2:
    case 3:
      tmp_expanded = extendBinaryFloatingPoint<T, ieee_754_2008::Binary32>(tmp);
      break;
    case 4:
      tmp_expanded = tmp;
      break;
    default:
      __builtin_unreachable();
    }

    out(col) = std::bit_cast<float>(tmp_expanded);
  }
}

} // namespace

// NOLINTNEXTLINE(modernize-avoid-c-arrays)
void DeflateDecompressor::decode(std::unique_ptr<unsigned char[]>* uBuffer,
                                 iPoint2D maxDim, iPoint2D dim, iPoint2D off) {
  int bytesps = bps / 8;
  invariant(bytesps >= 2 && bytesps <= 4);

  auto dstLen = implicit_cast<uLongf>(bytesps * maxDim.area());

  if (!*uBuffer) {
    // NOLINTNEXTLINE(modernize-avoid-c-arrays)
    *uBuffer = std::unique_ptr<unsigned char[]>(new unsigned char[dstLen]);
  }

  Array2DRef<unsigned char> tmp(uBuffer->get(), bytesps * maxDim.x, maxDim.y);

  if (int err =
          uncompress(uBuffer->get(), &dstLen, input.begin(), input.getSize());
      err != Z_OK) {
    ThrowRDE("failed to uncompress tile: %d (%s)", err, zError(err));
  }

  const auto out = CroppedArray2DRef(
      mRaw->getF32DataAsUncroppedArray2DRef(), /*offsetCols=*/off.x,
      /*offsetRows=*/off.y, /*croppedWidth=*/dim.x, /*croppedHeight=*/dim.y);

  for (int row = 0; row < out.croppedHeight; ++row) {
    decodeDeltaBytes(tmp[row], maxDim.x, bytesps, predFactor);

    switch (bytesps) {
    case 2:
      decodeFPDeltaRow<ieee_754_2008::Binary16>(tmp[row], maxDim.x, out[row]);
      break;
    case 3:
      decodeFPDeltaRow<ieee_754_2008::Binary24>(tmp[row], maxDim.x, out[row]);
      break;
    case 4:
      decodeFPDeltaRow<ieee_754_2008::Binary32>(tmp[row], maxDim.x, out[row]);
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
