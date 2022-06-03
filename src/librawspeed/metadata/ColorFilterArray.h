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

#include "common/Point.h" // for iPoint2D
#include <cstdint>        // for uint32_t
#include <map>            // for map
#include <string>         // for string
#include <vector>         // for vector

namespace rawspeed {

enum class CFAColor : uint8_t {
  // see also DngDecoder
  RED = 0,
  GREEN = 1,
  BLUE = 2,
  CYAN = 3,
  MAGENTA = 4,
  YELLOW = 5,
  WHITE = 6,
  FUJI_GREEN = 7,
  END, // keep it last!
  UNKNOWN = 255,

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

  // Compute the effective CFA after moving point-of-origin (element (0,0))
  // inwards towards image center this many pixels.
  void shiftRight(int n = 1);
  void shiftDown(int n = 1);

  [[nodiscard]] CFAColor getColorAt(int x, int y) const;
  [[nodiscard]] uint32_t getDcrawFilter() const;
  [[nodiscard]] std::string asString() const;
  [[nodiscard]] iPoint2D getSize() const { return size; }

  static std::string colorToString(CFAColor c);
  static uint32_t __attribute__((const))
  shiftDcrawFilter(uint32_t filter, int x, int y);

protected:
  static const std::map<CFAColor, std::string> color2String;
};

// FC macro from dcraw outputs, given the filters definition, the dcraw color
// number for that given position in the CFA pattern
// #define FC(filters,row,col) ((filters) >> ((((row) << 1 & 14) + ((col) & 1)) << 1) & 3)

} // namespace rawspeed
