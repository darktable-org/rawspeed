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
#include <string>          // for string

namespace RawSpeed {

enum CFAColor {
  // see also DngDecoder
  CFA_RED = 0,
  CFA_GREEN = 1,
  CFA_BLUE = 2,
  CFA_INVALID = 3, // keep the unit tests happy
  CFA_CYAN = 4,
  CFA_MAGENTA = 5,
  CFA_YELLOW = 6,
  CFA_WHITE = 7,
  CFA_FUJI_GREEN = 9,
  CFA_UNKNOWN = 255
};

class ColorFilterArray
{
  iPoint2D size;

public:
  ColorFilterArray();
  ColorFilterArray(const ColorFilterArray& other );
  ColorFilterArray& operator= (const ColorFilterArray& other);
  ColorFilterArray(const iPoint2D &size);
  ColorFilterArray(uint32 dcrawFilters);
  virtual ~ColorFilterArray();

  virtual void setSize(const iPoint2D &size);
  void setColorAt(iPoint2D pos, CFAColor c);
  virtual void setCFA(iPoint2D size, ...);
  virtual CFAColor getColorAt(uint32 x, uint32 y);
  virtual uint32 getDcrawFilter();
  virtual void shiftLeft(int n = 1);
  virtual void shiftDown(int n = 1);
  virtual std::string asString();
  iPoint2D getSize() const { return size; }

  static std::string colorToString(CFAColor c);
  uint32 toDcrawColor(CFAColor c);
  CFAColor toRawspeedColor(uint32 dcrawColor);
protected:
  CFAColor *cfa;
};

} // namespace RawSpeed
