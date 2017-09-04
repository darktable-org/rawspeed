/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2017 Roman Lebedev

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

#include <algorithm> // for generate_n, min
#include <cassert>   // for assert
#include <iterator>  // for back_insert_iterator
#include <numeric>   // for accumulate
#include <vector>    // for vector

namespace rawspeed {

inline std::vector<unsigned> sliceUp(unsigned bucketsNum, unsigned pieces) {
  std::vector<unsigned> buckets;

  if (!bucketsNum || !pieces)
    return buckets;

  bucketsNum = std::min(bucketsNum, pieces);
  buckets.reserve(bucketsNum);

  const auto quot = pieces / bucketsNum;
  auto rem = pieces % bucketsNum;

  std::generate_n(std::back_insert_iterator<std::vector<unsigned>>(buckets),
                  bucketsNum, [quot, &rem]() {
                    auto bucket = quot;
                    if (rem > 0) {
                      bucket++;
                      rem--;
                    }
                    return bucket;
                  });

  assert(std::accumulate(buckets.begin(), buckets.end(), 0UL) == pieces);

  return buckets;
}

} // namespace rawspeed
