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

#include <array>   // for array
#include <cstdint> // for uint16_t

namespace rawspeed {

class RawImage;

class Cr2sRawInterpolator final {
  const RawImage& mRaw;
  std::array<int, 3> sraw_coeffs;
  int hue;

  struct YCbCr;

public:
  Cr2sRawInterpolator(const RawImage& mRaw_, std::array<int, 3> sraw_coeffs_,
                      int hue_)
      : mRaw(mRaw_), sraw_coeffs(sraw_coeffs_), hue(hue_) {}

  void interpolate(int version);

protected:
  template <int version> inline void YUV_TO_RGB(const YCbCr& p, uint16_t* X);

  static inline void STORE_RGB(uint16_t* X, int r, int g, int b);

  template <int version> inline void interpolate_422_row(uint16_t* data, int w);
  template <int version> inline void interpolate_422(int w, int h);

  template <int version>
  inline void interpolate_420_row(std::array<uint16_t*, 3> line, int w);
  template <int version> inline void interpolate_420(int w, int h);
};

} // namespace rawspeed
