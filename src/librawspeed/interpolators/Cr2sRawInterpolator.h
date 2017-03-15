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

#pragma once

#include "common/Common.h" // for ushort16
#include <array>           // for array

namespace RawSpeed {

class RawImage;

class Cr2sRawInterpolator final {
  RawImage& mRaw;
  std::array<int, 3> sraw_coeffs;
  int raw_hue;

  struct YCbCr;

public:
  Cr2sRawInterpolator(RawImage& mRaw_, std::array<int, 3> sraw_coeffs_, int hue)
      : mRaw(mRaw_), sraw_coeffs(sraw_coeffs_), raw_hue(hue) {}

  void interpolate(int version);

protected:
  template <int version>
  inline void YUV_TO_RGB(int Y, int Cb, int Cr, ushort16* X, int offset);

  inline void STORE_RGB(ushort16* X, int r, int g, int b, int offset);

  template <int version>
  inline void interpolate_422_row(ushort16* data, int hue, int hue_last, int w);
  template <int version>
  inline void interpolate_422(int hue, int hue_last, int w, int h);

  template <int version> inline void interpolate_420(int hue, int w, int h);

  template <int version> void interpolate_422(int w, int h);
  template <int version> void interpolate_420(int w, int h);
};

} // namespace RawSpeed
