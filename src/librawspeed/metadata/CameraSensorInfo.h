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

#pragma once

#include "rawspeedconfig.h"
#include <vector>

namespace rawspeed {

class CameraSensorInfo final {
public:
  CameraSensorInfo(int black_level, int white_level, int min_iso, int max_iso,
                   std::vector<int> black_separate);
  [[nodiscard]] bool RAWSPEED_READONLY isIsoWithin(int iso) const;
  [[nodiscard]] bool RAWSPEED_READONLY isDefault() const;
  int mBlackLevel;
  int mWhiteLevel;
  int mMinIso;
  int mMaxIso;
  std::vector<int> mBlackLevelSeparate;
};

} // namespace rawspeed
