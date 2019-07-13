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
#include "common/Common.h"                // for int32_t, uint32, uint16_t
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

  validateStrips();
}

void PhaseOneDecompressor::validateStrips() const {
  // The 'strips' vector should contain exactly one element per row of image.

  // If the length is different, then the 'strips' vector is clearly incorrect.
  if (strips.size() != static_cast<decltype(strips)::size_type>(mRaw->dim.y)) {
    ThrowRDE("Height (%u) vs strip count %zu mismatch", mRaw->dim.y,
             strips.size());
  }

  struct RowBin {
    using value_type = unsigned char;
    bool isEmpty() const { return data == 0; }
    void fill() { data = 1; }
    value_type data = 0;
  };

  // Now, the strips in 'strips' vector aren't in order.
  // The 'decltype(strips)::value_type::n' is the row number of a strip.
  // We need to make sure that we have every row (0..mRaw->dim.y-1), once.

  // There are many ways to do that. Here, we take the histogram of all the
  // row numbers, and if any bin ends up not being '1' (one strip per row),
  // then the input is bad.
  std::vector<RowBin> histogram;
  histogram.resize(strips.size());
  int numBinsFilled = 0;
  std::for_each(strips.begin(), strips.end(),
                [y = mRaw->dim.y, &histogram,
                 &numBinsFilled](const PhaseOneStrip& strip) {
                  if (strip.n < 0 || strip.n >= y)
                    ThrowRDE("Strip specifies out-of-bounds row %u", strip.n);
                  RowBin& rowBin = histogram[strip.n];
                  if (!rowBin.isEmpty())
                    ThrowRDE("Duplicate row %u", strip.n);
                  rowBin.fill();
                  numBinsFilled++;
                });
  assert(histogram.size() == strips.size());
  assert(numBinsFilled == mRaw->dim.y &&
         "We should only get here if all the rows/bins got filled.");
}

void PhaseOneDecompressor::decompressStrip(const PhaseOneStrip& strip) const {
  uint32 width = mRaw->dim.x;
  assert(width % 2 == 0);

  static constexpr std::array<const int, 10> length = {8,  7, 6,  9,  11,
                                                       10, 5, 12, 14, 13};

  BitPumpMSB32 pump(strip.bs);

  std::array<int32_t, 2> pred;
  pred.fill(0);
  std::array<int, 2> len;
  auto* img = reinterpret_cast<uint16_t*>(mRaw->getData(0, strip.n));
  for (uint32 col = 0; col < width; col++) {
    pump.fill(32);
    if (col >= (width & ~7U)) // last 'width % 8' pixels.
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
      img[col] = pred[col & 1] = pump.getBitsNoFill(16);
    else {
      pred[col & 1] +=
          static_cast<signed>(pump.getBitsNoFill(i)) + 1 - (1 << (i - 1));
      // FIXME: is the truncation the right solution here?
      img[col] = uint16_t(pred[col & 1]);
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
