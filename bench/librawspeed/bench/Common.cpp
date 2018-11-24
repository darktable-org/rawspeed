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

#include "bench/Common.h"
#include "common/Common.h" // for roundUp
#include "common/Point.h"  // for iPoint2D
#include <cassert>         // for assert
#include <cmath>           // for ceil, sqrt

using rawspeed::iPoint2D;
using rawspeed::roundUp;
using std::sqrt;

iPoint2D __attribute__((const, visibility("default")))
areaToRectangle(size_t area, iPoint2D aspect) {
  double sqSide = sqrt(area);
  double sqARatio =
      sqrt(static_cast<double>(aspect.x) / static_cast<double>(aspect.y));

  iPoint2D dim(ceil(sqSide * sqARatio), ceil(sqSide / sqARatio));

  dim.x = roundUp(dim.x, aspect.x);
  dim.y = roundUp(dim.y, aspect.y);

  assert(dim.area() >= area);

  return dim;
}
