/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2017 Vasily Khoruzhick
    Copyright (C) 2020 Roman Lebedev

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

#include <cstdint> // for uint32_t, uint16_t

namespace rawspeed {

// Expand IEEE-754-2008 binary16 into float32
inline uint32_t fp16ToFloat(uint16_t fp16) {
  // IEEE-754-2008: binary16:
  // bit 15 - sign
  // bits 14-10 - exponent (5 bit)
  // bits 9-0 - fraction (10 bit)
  //
  // exp = 0, fract = +-0: zero
  // exp = 0; fract != 0: subnormal numbers
  //                      equation: -1 ^ sign * 2 ^ -14 * 0.fraction
  // exp = 1..30: normalized value
  //              equation: -1 ^ sign * 2 ^ (exponent - 15) * 1.fraction
  // exp = 31, fract = +-0: +-infinity
  // exp = 31, fract != 0: NaN

  uint32_t sign = (fp16 >> 15) & 1;
  uint32_t fp16_exponent = (fp16 >> 10) & ((1 << 5) - 1);
  uint32_t fp16_fraction = fp16 & ((1 << 10) - 1);

  // Normalized or zero
  // binary32 equation: -1 ^ sign * 2 ^ (exponent - 127) * 1.fraction
  // => exponent32 - 127 = exponent16 - 15, exponent32 = exponent16 + 127 - 15
  uint32_t fp32_exponent = fp16_exponent + 127 - 15;
  uint32_t fp32_fraction = fp16_fraction
                           << (23 - 10); // 23 is binary32 fraction size

  if (fp16_exponent == 31) {
    // Infinity or NaN
    fp32_exponent = 255;
  } else if (fp16_exponent == 0) {
    if (fp16_fraction == 0) {
      // +-Zero
      fp32_exponent = 0;
      fp32_fraction = 0;
    } else {
      // Subnormal numbers
      // binary32 equation: -1 ^ sign * 2 ^ (exponent - 127) * 1.fraction
      // binary16 equation: -1 ^ sign * 2 ^ -14 * 0.fraction, we can represent
      // it as a normalized value in binary32, we have to shift fraction until
      // we get 1.new_fraction and decrement exponent for each shift
      fp32_exponent = -14 + 127;
      while (!(fp32_fraction & (1 << 23))) {
        fp32_exponent -= 1;
        fp32_fraction <<= 1;
      }
      fp32_fraction &= ((1 << 23) - 1);
    }
  }
  return (sign << 31) | (fp32_exponent << 23) | fp32_fraction;
}

// Expand binary24 (not part of IEEE-754-2008) into float32
inline uint32_t fp24ToFloat(uint32_t fp24) {
  // binary24: Not a part of IEEE754-2008, but format is obvious,
  // see https://en.wikipedia.org/wiki/Minifloat
  // bit 23 - sign
  // bits 22-16 - exponent (7 bit)
  // bits 15-0 - fraction (16 bit)
  //
  // exp = 0, fract = +-0: zero
  // exp = 0; fract != 0: subnormal numbers
  //                      equation: -1 ^ sign * 2 ^ -62 * 0.fraction
  // exp = 1..126: normalized value
  //              equation: -1 ^ sign * 2 ^ (exponent - 63) * 1.fraction
  // exp = 127, fract = +-0: +-infinity
  // exp = 127, fract != 0: NaN

  uint32_t sign = (fp24 >> 23) & 1;
  uint32_t fp24_exponent = (fp24 >> 16) & ((1 << 7) - 1);
  uint32_t fp24_fraction = fp24 & ((1 << 16) - 1);

  // Normalized or zero
  // binary32 equation: -1 ^ sign * 2 ^ (exponent - 127) * 1.fraction
  // => exponent32 - 127 = exponent24 - 64, exponent32 = exponent16 + 127 - 63
  uint32_t fp32_exponent = fp24_exponent + 127 - 63;
  uint32_t fp32_fraction = fp24_fraction
                           << (23 - 16); // 23 is binary 32 fraction size

  if (fp24_exponent == 127) {
    // Infinity or NaN
    fp32_exponent = 255;
  } else if (fp24_exponent == 0) {
    if (fp24_fraction == 0) {
      // +-Zero
      fp32_exponent = 0;
      fp32_fraction = 0;
    } else {
      // Subnormal numbers
      // binary32 equation: -1 ^ sign * 2 ^ (exponent - 127) * 1.fraction
      // binary24 equation: -1 ^ sign * 2 ^ -62 * 0.fraction, we can represent
      // it as a normalized value in binary32, we have to shift fraction until
      // we get 1.new_fraction and decrement exponent for each shift
      fp32_exponent = -62 + 127;
      while (!(fp32_fraction & (1 << 23))) {
        fp32_exponent -= 1;
        fp32_fraction <<= 1;
      }
      fp32_fraction &= ((1 << 23) - 1);
    }
  }
  return (sign << 31) | (fp32_exponent << 23) | fp32_fraction;
}

} // namespace rawspeed
