/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2014 Pedro Côrte-Real
    Copyright (C) 2017-2019 Roman Lebedev

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

#include "rawspeedconfig.h" // for HAVE_OPENMP
#include "decompressors/SonyArw2Decompressor.h"
#include "common/Array2DRef.h"            // for Array2DRef
#include "common/Common.h"                // for rawspeed_get_number_of_pro...
#include "common/Point.h"                 // for iPoint2D
#include "common/RawImage.h"              // for RawImageData, RawImage
#include "common/RawspeedException.h"     // for RawspeedException
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "io/BitPumpLSB.h"                // for BitPumpLSB
#include <cassert>                        // for assert
#include <cstdint>                        // for uint16_t, uint32_t, uint8_t
#include <string>                         // for string

namespace rawspeed {

SonyArw2Decompressor::SonyArw2Decompressor(const RawImage& img,
                                           const ByteStream& input_)
    : mRaw(img) {
  if (mRaw->getCpp() != 1 || mRaw->getDataType() != RawImageType::UINT16 ||
      mRaw->getBpp() != sizeof(uint16_t))
    ThrowRDE("Unexpected component count / data type");

  if (!mRaw->dim.hasPositiveArea() || mRaw->dim.x % 32 != 0 ||
      mRaw->dim.x > 9600 || mRaw->dim.y > 6376)
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", mRaw->dim.x,
             mRaw->dim.y);

  // 1 byte per pixel
  input = input_.peekStream(mRaw->dim.x * mRaw->dim.y);
}

void SonyArw2Decompressor::decompressRow(int row) const {
  const Array2DRef<uint16_t> out(mRaw->getU16DataAsUncroppedArray2DRef());
  assert(out.width > 0);
  assert(out.width % 32 == 0);

  // Allow compiler to devirtualize the calls below.
  auto& rawdata = reinterpret_cast<RawImageDataU16&>(*mRaw);

  ByteStream rowBs = input;
  rowBs.skipBytes(row * out.width);
  rowBs = rowBs.peekStream(out.width);

  BitPumpLSB bits(rowBs);

  uint32_t random = bits.peekBits(24);

  // Each loop iteration processes 16 pixels, consuming 128 bits of input.
  for (int col = 0; col < out.width; col += ((col & 1) != 0) ? 31 : 1) {
    // 30 bits.
    int _max = bits.getBits(11);
    int _min = bits.getBits(11);
    int _imax = bits.getBits(4);
    int _imin = bits.getBits(4);

    // 128-30 = 98 bits remaining, still need to decode 16 pixels...
    // Each full pixel consumes 7 bits, thus we can only have 14 full pixels.
    // So we lack 2 pixels. That is where _imin and _imax come into play,
    // values of those pixels were already specified in _min and _max.
    // But what that means is, _imin and _imax must not be equal!
    if (_imax == _imin)
      ThrowRDE("ARW2 invariant failed, same pixel is both min and max");

    int sh = 0;
    while ((sh < 4) && ((0x80 << sh) <= (_max - _min)))
      sh++;

    for (int i = 0; i < 16; i++) {
      int p;
      if (i == _imax)
        p = _max;
      else {
        if (i == _imin)
          p = _min;
        else {
          p = (bits.getBits(7) << sh) + _min;
          if (p > 0x7ff)
            p = 0x7ff;
        }
      }
      rawdata.setWithLookUp(
          p << 1, reinterpret_cast<uint8_t*>(&out(row, col + i * 2)), &random);
    }
  }
}

void SonyArw2Decompressor::decompressThread() const noexcept {
  assert(mRaw->dim.x > 0);
  assert(mRaw->dim.x % 32 == 0);
  assert(mRaw->dim.y > 0);

#ifdef HAVE_OPENMP
#pragma omp for schedule(static)
#endif
  for (int y = 0; y < mRaw->dim.y; y++) {
    try {
      decompressRow(y);
    } catch (const RawspeedException& err) {
      // Propagate the exception out of OpenMP magic.
      mRaw->setError(err.what());
#ifdef HAVE_OPENMP
#pragma omp cancel for
#endif
    }
  }
}

void SonyArw2Decompressor::decompress() const {
#ifdef HAVE_OPENMP
#pragma omp parallel default(none)                                             \
    num_threads(rawspeed_get_number_of_processor_cores())
#endif
  decompressThread();

  std::string firstErr;
  if (mRaw->isTooManyErrors(1, &firstErr)) {
    ThrowRDE("Too many errors encountered. Giving up. First Error:\n%s",
             firstErr.c_str());
  }
}

} // namespace rawspeed
