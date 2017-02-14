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

#include "metadata/ColorFilterArray.h"
#include "common/Common.h"                // for writeLog, uint32, DEBUG_PR...
#include "common/Point.h"                 // for iPoint2D
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include <algorithm>                      // for fill
#include <cstdarg>                        // for va_arg, va_end, va_list
#include <cstdlib>                        // for size_t, abs
#include <map>                            // for map
#include <stdexcept>                      // for out_of_range
#include <string>                         // for string, allocator

using namespace std;

namespace RawSpeed {

ColorFilterArray::ColorFilterArray(const iPoint2D &_size) {
  setSize(_size);
}

void ColorFilterArray::setSize(const iPoint2D& _size)
{
  size = _size;

  if (size.area() > 36) {
    // Bayer, FC() supports 2x8 pattern
    // X-Trans is 6x6 pattern
    // is there anything bigger?
    ThrowRDE("ColorFilterArray:setSize if your CFA pattern is really %d pixels "
             "in area we may as well give up now",
             size.area());
  }
  if (size.area() <= 0)
    return;
  cfa.resize(size.area());
  fill(cfa.begin(), cfa.end(), CFA_UNKNOWN);
}

CFAColor ColorFilterArray::getColorAt( int x, int y ) const
{
  if (cfa.empty())
    ThrowRDE("ColorFilterArray:getColorAt: No CFA size set");

  // calculate the positive modulo [0 .. size-1]
  x = (x % size.x + size.x) % size.x;
  y = (y % size.y + size.y) % size.y;

  return cfa[x + (size_t)y * size.x];
}

void ColorFilterArray::setCFA( iPoint2D in_size, ... )
{
  if (in_size != size) {
    setSize(in_size);
  }
  va_list arguments;
  va_start(arguments, in_size);
  for (uint32 i = 0; i <  size.area(); i++ ) {
    cfa[i] = (CFAColor)va_arg(arguments, int);
  }
  va_end (arguments);
}

void ColorFilterArray::shiftLeft(int n) {
  if (cfa.empty())
    ThrowRDE("ColorFilterArray:shiftLeft: No CFA size set (or set to zero)");

  writeLog(DEBUG_PRIO_EXTRA, "Shift left:%d\n", n);
  n %= size.x;
  if (n == 0)
    return;

  vector<CFAColor> tmp(size.area());
  for (int y = 0; y < size.y; ++y) {
    for (int x = 0; x < size.x; ++x) {
      tmp[x + (size_t)y * size.x] = getColorAt(x + n, y);
    }
  }
  cfa = tmp;
}

void ColorFilterArray::shiftDown(int n) {
  if (cfa.empty())
    ThrowRDE("ColorFilterArray:shiftDown: No CFA size set (or set to zero)");

  writeLog(DEBUG_PRIO_EXTRA, "Shift down:%d\n", n);
  n %= size.y;
  if (n == 0)
    return;
  vector<CFAColor> tmp(size.area());
  for (int y = 0; y < size.y; ++y) {
    for (int x = 0; x < size.x; ++x) {
      tmp[x + (size_t)y * size.x] = getColorAt(x, y + n);
    }
  }
  cfa = tmp;
}

string ColorFilterArray::asString() const {
  string dst;
  for (int y = 0; y < size.y; y++) {
    for (int x = 0; x < size.x; x++) {
      dst += colorToString(getColorAt(x,y));
      dst += (x == size.x - 1) ? "\n" : ",";
    }
  }
  return dst;
}

uint32 ColorFilterArray::shiftDcrawFilter(uint32 filter, int x, int y)
{
  // filter is a series of 4 bytes that describe a 2x8 matrix. 2 is the width,
  // 8 is the height. each byte describes a 2x2 pixel block. so each pixel has
  // 2 bits of information. This allows to distinguish 4 different colors.

  if (std::abs(x) & 1) {
    // A shift in x direction means swapping the first and second half of each
    // nibble.
    // see http://graphics.stanford.edu/~seander/bithacks.html#SwappingBitsXOR
    for (int n = 0; n < 8; ++n) {
      int i = n * 4;
      int j = i + 2;
      uint32 t = ((filter >> i) ^ (filter >> j)) & ((1U << 2) - 1);
      filter = filter ^ ((t << i) | (t << j));
    }
  }

  if (y == 0)
    return filter;

  // A shift in y direction means rotating the whole int by 4 bits.
  y *= 4;
  y = y >= 0 ? y % 32 : 32 - ((-y) % 32);
  filter = (filter >> y) | (filter << (32 - y));

  return filter;
}

const static map<CFAColor, string> color2String = {
    {CFA_RED, "RED"},         {CFA_GREEN, "GREEN"},
    {CFA_BLUE, "BLUE"},       {CFA_CYAN, "CYAN"},
    {CFA_MAGENTA, "MAGENTA"}, {CFA_YELLOW, "YELLOW"},
    {CFA_WHITE, "WHITE"},     {CFA_FUJI_GREEN, "FUJIGREEN"},
    {CFA_UNKNOWN, "UNKNOWN"}
};

string ColorFilterArray::colorToString(CFAColor c)
{
  try {
    return color2String.at(c);
  } catch (std::out_of_range&) {
    ThrowRDE("ColorFilterArray: Unsupported CFA Color: %u", c);
  }
}

void ColorFilterArray::setColorAt(iPoint2D pos, CFAColor c) {
  if (pos.x >= size.x || pos.x < 0)
    ThrowRDE("ColorFilterArray::SetColor: position out of CFA pattern");
  if (pos.y >= size.y || pos.y < 0)
    ThrowRDE("ColorFilterArray::SetColor: position out of CFA pattern");
  cfa[pos.x + (size_t)pos.y * size.x] = c;
}

static uint32 toDcrawColor( CFAColor c )
{
  switch (c) {
  case CFA_FUJI_GREEN:
  case CFA_RED: return 0;
  case CFA_MAGENTA:
  case CFA_GREEN: return 1;
  case CFA_CYAN:
  case CFA_BLUE: return 2;
  case CFA_YELLOW: return 3;
  default:
    throw out_of_range(ColorFilterArray::colorToString(c));
  }
}

uint32 ColorFilterArray::getDcrawFilter() const
{
  //dcraw magic
  if (size.x == 6 && size.y == 6)
    return 9;

  if (cfa.empty() || size.x > 2 || size.y > 8 || !isPowerOfTwo(size.y))
    return 1;

  uint32 ret = 0;
  for (int x = 0; x < 2; x++) {
    for (int y = 0; y < 8; y++) {
      uint32 c = toDcrawColor(getColorAt(x,y));
      int g = (x >> 1) * 8;
      ret |= c << ((x&1)*2 + y*4 + g);
    }
  }
  for (int y = 0; y < size.y; y++) {
    for (int x = 0; x < size.x; x++) {
      writeLog(DEBUG_PRIO_EXTRA, "%s,", colorToString((CFAColor)toDcrawColor(getColorAt(x,y))).c_str());
    }
    writeLog(DEBUG_PRIO_EXTRA, "\n");
  }
  writeLog(DEBUG_PRIO_EXTRA, "DCRAW filter:%x\n",ret);
  return ret;
}

} // namespace RawSpeed
