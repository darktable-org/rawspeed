/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2014-2015 Pedro Côrte-Real
    Copyright (C) 2017-2018 Roman Lebedev

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
#include "decompressors/PhaseOneDecompressor.h"
#include "common/Common.h"                // for int32_t, uint32_t, uint16_t
#include "common/Point.h"                 // for iPoint2D
#include "common/RawImage.h"              // for RawImage, RawImageData
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "io/BitPumpMSB32.h"              // for BitPumpMSB32
#include <algorithm>                      // for for_each
#include <array>                          // for array
#include <cassert>                        // for assert
#include <cstddef>                        // for size_t
#include <utility>                        // for move
#include <vector>                         // for vector, vector<>::size_type

namespace rawspeed {

PhaseOneDecompressor::PhaseOneDecompressor(const RawImage& img,
                                           std::vector<PhaseOneStrip>&& strips_)
    : mRaw(img), strips(std::move(strips_)) {
  if (mRaw->getDataType() != TYPE_USHORT16)
    ThrowRDE("Unexpected data type");

  if (!((mRaw->getCpp() == 1 && mRaw->getBpp() == 2)))
    ThrowRDE("Unexpected cpp: %u", mRaw->getCpp());

  if (!mRaw->dim.hasPositiveArea() || mRaw->dim.x % 2 != 0 ||
      mRaw->dim.x > 11976 || mRaw->dim.y > 8852) {
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", mRaw->dim.x,
             mRaw->dim.y);
  }

  prepareStrips();
}

void PhaseOneDecompressor::prepareStrips() {
  // The 'strips' vector should contain exactly one element per row of image.

  // If the length is different, then the 'strips' vector is clearly incorrect.
  if (strips.size() != static_cast<decltype(strips)::size_type>(mRaw->dim.y)) {
    ThrowRDE("Height (%u) vs strip count %zu mismatch", mRaw->dim.y,
             strips.size());
  }

  // Now, the strips in 'strips' vector aren't in order.
  // The 'decltype(strips)::value_type::n' is the row number of a strip.
  // We need to make sure that we have every row (0..mRaw->dim.y-1), once.
  // For that, first let's sort them to have monothonically increasting `n`.
  // This will also serialize the per-line outputting.
  std::sort(
      strips.begin(), strips.end(),
      [](const PhaseOneStrip& a, const PhaseOneStrip& b) { return a.n < b.n; });
  // And now ensure that slice number matches the slice's row.
  for (decltype(strips)::size_type i = 0; i < strips.size(); ++i)
    if (static_cast<decltype(strips)::size_type>(strips[i].n) != i)
      ThrowRDE("Strips validation issue.");
  // All good.
}

void PhaseOneDecompressor::decompressStrip(const PhaseOneStrip& strip) const {
  const Array2DRef<uint16_t> out(mRaw->getU16DataAsUncroppedArray2DRef());

  assert(out.width > 0);
  assert(out.width % 2 == 0);

  static constexpr std::array<const int, 10> length = {8,  7, 6,  9,  11,
                                                       10, 5, 12, 14, 13};

  BitPumpMSB32 pump(strip.bs);

  std::array<int32_t, 2> pred;
  pred.fill(0);
  std::array<int, 2> len;
  const int row = strip.n;
  for (int col = 0; col < out.width; col++) {
    pump.fill(32);
    if (static_cast<unsigned>(col) >=
        (out.width & ~7U)) // last 'width % 8' pixels.
      len[0] = len[1] = 14;
    else if ((col & 7) == 0) {
      for (int& i : len) {
        int j = 0;

        for (; j < 5; j++) {
          if (pump.getBitsNoFill(1) != 0) {
            if (col == 0)
              ThrowRDE("Can not initialize lengths. Data is corrupt.");

            // else, we have previously initialized lengths, so we are fine
            break;
          }
        }

        assert((col == 0 && j > 0) || col != 0);
        if (j > 0)
          i = length[2 * (j - 1) + pump.getBitsNoFill(1)];
      }
    }

    int i = len[col & 1];
    if (i == 14)
      out(row, col) = pred[col & 1] = pump.getBitsNoFill(16);
    else {
      pred[col & 1] +=
          static_cast<signed>(pump.getBitsNoFill(i)) + 1 - (1 << (i - 1));
      // FIXME: is the truncation the right solution here?
      out(row, col) = uint16_t(pred[col & 1]);
    }
  }
}

void PhaseOneDecompressor::decompressThread() const noexcept {
#ifdef HAVE_OPENMP
#pragma omp for schedule(static)
#endif
  for (auto strip = strips.cbegin(); strip < strips.cend(); ++strip) {
    try {
      decompressStrip(*strip);
    } catch (RawspeedException& err) {
      // Propagate the exception out of OpenMP magic.
      mRaw->setError(err.what());
    }
  }
}

void PhaseOneDecompressor::decompress() const {
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
