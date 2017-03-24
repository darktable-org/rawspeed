/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post

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

#include "common/Common.h" // for uint32
#include "common/Point.h"  // for iPoint2D
#include <map>             // for map
#include <string>          // for string
#include <vector>          // for vector

namespace RawSpeed {

enum CFAColor {
  // see also DngDecoder
  CFA_RED = 0,
  CFA_GREEN = 1,
  CFA_BLUE = 2,
  CFA_CYAN = 3,
  CFA_MAGENTA = 4,
  CFA_YELLOW = 5,
  CFA_WHITE = 6,
  CFA_FUJI_GREEN = 7,
  CFA_UNKNOWN = 255
};

class ColorFilterArray
{
  std::vector<CFAColor> cfa;
  iPoint2D size;

public:
  ColorFilterArray() = default;
  explicit ColorFilterArray(const iPoint2D& size);

  void setSize(const iPoint2D& size);
  void setColorAt(iPoint2D pos, CFAColor c);
  void setCFA(iPoint2D size, ...);
  void shiftLeft(int n = 1);
  void shiftDown(int n = 1);

  CFAColor getColorAt(int x, int y) const;
  uint32 getDcrawFilter() const;
  std::string asString() const;
  iPoint2D getSize() const { return size; }

  static std::string colorToString(CFAColor c);
  static uint32 __attribute__((const))
  shiftDcrawFilter(uint32 filter, int x, int y);

protected:
  static const std::map<CFAColor, std::string> color2String;
};

// FC macro from dcraw outputs, given the filters definition, the dcraw color
// number for that given position in the CFA pattern
// #define FC(filters,row,col) ((filters) >> ((((row) << 1 & 14) + ((col) & 1)) << 1) & 3)

} // namespace RawSpeed
