#pragma once
/* 
    RawSpeed - RAW file decoder.

    Copyright (C) 2009 Klaus Post

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

    http://www.klauspost.com
*/

class RgbImage
{
public:
  RgbImage(void) : w(0), h(0), pitch(0), bpp(0), data(0), owned(false) {}
  RgbImage(RgbImage &i) : w(i.w), h(i.h), pitch(i.pitch), bpp(i.pitch), data(i.data), owned(false) {}
  RgbImage(int w, int h, int bpp);
  RgbImage(int w, int h, int bpp, int pitch, unsigned char* data);

  virtual ~RgbImage(void);
  const bool owned;
  const int w,h;
  int pitch;
  const int bpp;
  unsigned char* data;
};
