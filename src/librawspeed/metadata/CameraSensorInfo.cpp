/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2011 Klaus Post

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
#include "metadata/CameraSensorInfo.h"
#include <utility>
#include <vector>

using std::vector;

namespace rawspeed {

CameraSensorInfo::CameraSensorInfo(int black_level, int white_level,
                                   int min_iso, int max_iso,
                                   vector<int> black_separate)
    : mBlackLevel(black_level), mWhiteLevel(white_level), mMinIso(min_iso),
      mMaxIso(max_iso), mBlackLevelSeparate(std::move(black_separate)) {}

bool RAWSPEED_READONLY CameraSensorInfo::isIsoWithin(int iso) const {
  return (iso >= mMinIso && iso <= mMaxIso) || (iso >= mMinIso && 0 == mMaxIso);
}

bool RAWSPEED_READONLY CameraSensorInfo::isDefault() const {
  return (0 == mMinIso && 0 == mMaxIso);
}

} // namespace rawspeed
