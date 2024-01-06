/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2024 Roman Lebedev

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

#pragma once

#include "adt/Array1DRef.h"
#include "adt/Invariant.h"
#include <algorithm>
#include <cstdint>
#include <cstring>

namespace rawspeed {

inline void variableLengthLoadNaiveViaConditionalLoad(
    Array1DRef<uint8_t> out, Array1DRef<const uint8_t> in, int inPos) {
  invariant(out.size() != 0);
  invariant(in.size() != 0);
  invariant(out.size() <= in.size());
  invariant(inPos >= 0);

  std::fill(out.begin(), out.end(), 0);

  for (int outIndex = 0; outIndex != out.size(); ++outIndex) {
    const int inIndex = inPos + outIndex;
    if (inIndex >= in.size())
      return;
    out(outIndex) = in(inIndex); // masked load
  }
}

inline void variableLengthLoadNaiveViaStdCopy(Array1DRef<uint8_t> out,
                                              Array1DRef<const uint8_t> in,
                                              int inPos) {
  invariant(out.size() != 0);
  invariant(in.size() != 0);
  invariant(out.size() <= in.size());
  invariant(inPos >= 0);

  inPos = std::min(inPos, in.size());

  int inPosEnd = inPos + out.size();
  inPosEnd = std::min(inPosEnd, in.size());
  invariant(inPos <= inPosEnd);

  const int copySize = inPosEnd - inPos;
  invariant(copySize >= 0);
  invariant(copySize <= out.size());

  std::fill(out.begin(), out.end(), 0);
  std::copy(in.addressOf(inPos), in.addressOf(inPosEnd), out.begin());
}

inline void variableLengthLoadNaiveViaMemcpy(Array1DRef<uint8_t> out,
                                             Array1DRef<const uint8_t> in,
                                             int inPos) {
  invariant(out.size() != 0);
  invariant(in.size() != 0);
  invariant(out.size() <= in.size());
  invariant(inPos >= 0);

  std::fill(out.begin(), out.end(), 0);

  // How many bytes are left in input buffer?
  // Since pos can be past-the-end we need to carefully handle overflow.
  int bytesRemaining = (inPos < in.size()) ? in.size() - inPos : 0;
  // And if we are not at the end of the input, we may have more than we need.
  bytesRemaining = std::min<int>(out.size(), bytesRemaining);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wunsafe-buffer-usage"
  memcpy(out.begin(), in.begin() + inPos, bytesRemaining);
#pragma GCC diagnostic pop
}

} // namespace rawspeed
