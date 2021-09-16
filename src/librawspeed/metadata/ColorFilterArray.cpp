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
#include "common/Common.h"                // for writeLog, DEBUG_PRIO::EXTRA
#include "common/Point.h"                 // for iPoint2D, iPoint2D::value_...
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include <algorithm>                      // for fill
#include <cinttypes>                      // for PRId64
#include <cstdarg>                        // for va_arg, va_end, va_list
#include <cstdlib>                        // for size_t, abs
#include <map>                            // for map
#include <stdexcept>                      // for out_of_range
#include <string>                         // for string

using std::vector;

using std::out_of_range;
using std::map;

namespace rawspeed {

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
    ThrowRDE("if your CFA pattern is really %" PRId64 " pixels "
             "in area we may as well give up now",
             size.area());
  }
  if (size.area() <= 0)
    return;
  cfa.resize(size.area());
  fill(cfa.begin(), cfa.end(), CFAColor::UNKNOWN);
}

CFAColor ColorFilterArray::getColorAt( int x, int y ) const
{
  if (cfa.empty())
    ThrowRDE("No CFA size set");

  // calculate the positive modulo [0 .. size-1]
  x = (x % size.x + size.x) % size.x;
  y = (y % size.y + size.y) % size.y;

  return cfa[x + static_cast<size_t>(y) * size.x];
}

void ColorFilterArray::setCFA( iPoint2D in_size, ... )
{
  if (in_size != size) {
    setSize(in_size);
  }
  va_list arguments;
  va_start(arguments, in_size);
  for (auto i = 0UL; i < size.area(); i++) {
    cfa[i] = static_cast<CFAColor>(va_arg(arguments, int));
  }
  va_end (arguments);
}

void ColorFilterArray::shiftLeft(int n) {
  if (cfa.empty())
    ThrowRDE("No CFA size set (or set to zero)");

  writeLog(DEBUG_PRIO::EXTRA, "Shift left:%d", n);
  n %= size.x;
  if (n == 0)
    return;

  vector<CFAColor> tmp(size.area());
  for (int y = 0; y < size.y; ++y) {
    for (int x = 0; x < size.x; ++x) {
      tmp[x + static_cast<size_t>(y) * size.x] = getColorAt(x + n, y);
    }
  }
  cfa = tmp;
}

void ColorFilterArray::shiftDown(int n) {
  if (cfa.empty())
    ThrowRDE("No CFA size set (or set to zero)");

  writeLog(DEBUG_PRIO::EXTRA, "Shift down:%d", n);
  n %= size.y;
  if (n == 0)
    return;
  vector<CFAColor> tmp(size.area());
  for (int y = 0; y < size.y; ++y) {
    for (int x = 0; x < size.x; ++x) {
      tmp[x + static_cast<size_t>(y) * size.x] = getColorAt(x, y + n);
    }
  }
  cfa = tmp;
}

std::string ColorFilterArray::asString() const {
  std::string dst;
  for (int y = 0; y < size.y; y++) {
    for (int x = 0; x < size.x; x++) {
      dst += colorToString(getColorAt(x,y));
      dst += (x == size.x - 1) ? "\n" : ",";
    }
  }
  return dst;
}

uint32_t ColorFilterArray::shiftDcrawFilter(uint32_t filter, int x, int y) {
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
      uint32_t t = ((filter >> i) ^ (filter >> j)) & ((1U << 2) - 1);
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

const map<CFAColor, std::string> ColorFilterArray::color2String = {
    {CFAColor::RED, "RED"},         {CFAColor::GREEN, "GREEN"},
    {CFAColor::BLUE, "BLUE"},       {CFAColor::CYAN, "CYAN"},
    {CFAColor::MAGENTA, "MAGENTA"}, {CFAColor::YELLOW, "YELLOW"},
    {CFAColor::WHITE, "WHITE"},     {CFAColor::FUJI_GREEN, "FUJIGREEN"},
    {CFAColor::UNKNOWN, "UNKNOWN"}};

std::string ColorFilterArray::colorToString(CFAColor c) {
  try {
    return color2String.at(c);
  } catch (const std::out_of_range&) {
    ThrowRDE("Unsupported CFA Color: %u", static_cast<unsigned>(c));
  }
}

void ColorFilterArray::setColorAt(iPoint2D pos, CFAColor c) {
  if (pos.x >= size.x || pos.x < 0)
    ThrowRDE("position out of CFA pattern");
  if (pos.y >= size.y || pos.y < 0)
    ThrowRDE("position out of CFA pattern");
  cfa[pos.x + static_cast<size_t>(pos.y) * size.x] = c;
}

static uint32_t toDcrawColor(CFAColor c) {
  switch (c) {
  case CFAColor::FUJI_GREEN:
  case CFAColor::RED:
    return 0;
  case CFAColor::MAGENTA:
  case CFAColor::GREEN:
    return 1;
  case CFAColor::CYAN:
  case CFAColor::BLUE:
    return 2;
  case CFAColor::YELLOW:
    return 3;
  default:
    throw out_of_range(ColorFilterArray::colorToString(c));
  }
}

uint32_t ColorFilterArray::getDcrawFilter() const {
  //dcraw magic
  if (size.x == 6 && size.y == 6)
    return 9;

  if (cfa.empty() || size.x > 2 || size.y > 8 || !isPowerOfTwo(size.y))
    return 1;

  uint32_t ret = 0;
  for (int x = 0; x < 2; x++) {
    for (int y = 0; y < 8; y++) {
      uint32_t c = toDcrawColor(getColorAt(x, y));
      int g = (x >> 1) * 8;
      ret |= c << ((x&1)*2 + y*4 + g);
    }
  }

  writeLog(DEBUG_PRIO::EXTRA, "%s", asString().c_str());
  writeLog(DEBUG_PRIO::EXTRA, "DCRAW filter:%x", ret);

  return ret;
}

} // namespace rawspeed
