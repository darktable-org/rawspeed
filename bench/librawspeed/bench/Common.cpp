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

#include "rawspeedconfig.h"
#include "bench/Common.h"
#include "adt/Casts.h"
#include "adt/Point.h"
#include "common/Common.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>

using rawspeed::iPoint2D;
using rawspeed::roundUp;
using std::sqrt;

bool RAWSPEED_READNONE __attribute__((visibility("default")))
benchmarkDryRun() {
  // NOLINTNEXTLINE(concurrency-mt-unsafe)
  return std::getenv("RAWSPEED_BENCHMARK_DRYRUN") != nullptr;
}

iPoint2D RAWSPEED_READNONE __attribute__((visibility("default")))
areaToRectangle(uint64_t area, iPoint2D aspect) {
  double sqSide = sqrt(area);
  double sqARatio =
      sqrt(static_cast<double>(aspect.x) / static_cast<double>(aspect.y));

  iPoint2D dim(rawspeed::implicit_cast<int>(ceil(sqSide * sqARatio)),
               rawspeed::implicit_cast<int>(ceil(sqSide / sqARatio)));

  dim.x = rawspeed::implicit_cast<int>(roundUp(dim.x, aspect.x));
  dim.y = rawspeed::implicit_cast<int>(roundUp(dim.y, aspect.y));

  assert(dim.area() >= area);

  return dim;
}
