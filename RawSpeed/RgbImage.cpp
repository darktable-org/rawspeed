#include "StdAfx.h"
#include "RgbImage.h"
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

namespace RawSpeed {

RgbImage::RgbImage(int _w, int _h, int _bpp) : w(_w), h(_h), bpp(_bpp), owned(true) {
  pitch = ((w * bpp + 15) / 16) * 16;
  data = (unsigned char*)_aligned_malloc(pitch * h, 16);
}

RgbImage::RgbImage(int _w, int _h, int _bpp, int _pitch, unsigned char* _data) :
    w(_w), h(_h), bpp(_bpp), pitch(_pitch), data(_data), owned(false) {

}


RgbImage::~RgbImage(void) {
  if (data && owned) {
    _aligned_free(data);
  }
}

} // namespace RawSpeed
