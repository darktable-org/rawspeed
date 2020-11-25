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

namespace ieee_754_2008 {

// Refer to "3.6 Interchange format parameters",
//          "Table 3.5—Binary interchange format parameters"

// All formats are:
// MSB [Sign bit] [Exponent bits] [Fraction bits] LSB

template <int StorageWidth_, int FractionWidth_, int ExponentWidth_>
struct BinaryN {
  static constexpr uint32_t StorageWidth = StorageWidth_;

  // FIXME: if we had compile-time log2/round, we'd only need StorageWidth.

  static constexpr uint32_t FractionWidth = FractionWidth_;
  static constexpr uint32_t ExponentWidth = ExponentWidth_;
  // SignWidth is always 1.
  static_assert(FractionWidth + ExponentWidth + 1 == StorageWidth, "");

  static constexpr uint32_t Precision = FractionWidth + 1;

  static constexpr uint32_t ExponentMax = (1 << (ExponentWidth - 1)) - 1;

  static constexpr int32_t Bias = ExponentMax;

  // FractionPos is always 0.
  static constexpr uint32_t ExponentPos = FractionWidth;
  static constexpr uint32_t SignBitPos = StorageWidth - 1;
};

// IEEE-754-2008: binary16:
// bits 9-0 - fraction (10 bit)
// bits 14-10 - exponent (5 bit)
// bit 15 - sign
struct Binary16 : public BinaryN</*StorageWidth=*/16, /*FractionWidth=*/10,
                                 /*ExponentWidth=*/5> {
  static_assert(Precision == 11, "");
  static_assert(ExponentMax == 15, "");
  static_assert(ExponentPos == 10, "");
  static_assert(SignBitPos == 15, "");
};

// IEEE-754-2008: binary24:
// bits 15-0 - fraction (16 bit)
// bits 22-16 - exponent (7 bit)
// bit 23 - sign
struct Binary24 : public BinaryN</*StorageWidth=*/24, /*FractionWidth=*/16,
                                 /*ExponentWidth=*/7> {
  static_assert(Precision == 17, "");
  static_assert(ExponentMax == 63, "");
  static_assert(ExponentPos == 16, "");
  static_assert(SignBitPos == 23, "");
};

// IEEE-754-2008: binary32:
// bits 22-0 - fraction (23 bit)
// bits 30-23 - exponent (8 bit)
// bit 31 - sign
struct Binary32 : public BinaryN</*StorageWidth=*/32, /*FractionWidth=*/23,
                                 /*ExponentWidth=*/8> {
  static_assert(Precision == 24, "");
  static_assert(ExponentMax == 127, "");
  static_assert(ExponentPos == 23, "");
  static_assert(SignBitPos == 31, "");
};

// exp = 0, fract  = +-0: zero
// exp = 0; fract !=   0: subnormal numbers
//                        equation: -1 ^ sign * 2 ^ (1 - Bias) * 0.fraction
// exp = 1..(2^ExponentWidth - 2): normalized value
//                     equation: -1 ^ sign * 2 ^ (exponent - Bias) * 1.fraction
// exp = 2^ExponentWidth - 1, fract  = +-0: +-infinity
// exp = 2^ExponentWidth - 1, fract !=   0: NaN

} // namespace ieee_754_2008

template <typename NarrowType, typename WideType>
inline uint32_t extendBinaryFloatingPoint(uint32_t narrow) {
  uint32_t sign = (narrow >> NarrowType::SignBitPos) & 1;
  uint32_t narrow_exponent = (narrow >> NarrowType::ExponentPos) &
                             ((1 << NarrowType::ExponentWidth) - 1);
  uint32_t narrow_fraction = narrow & ((1 << NarrowType::FractionWidth) - 1);

  // Normalized or zero
  uint32_t wide_exponent =
      static_cast<int32_t>(narrow_exponent) - NarrowType::Bias + WideType::Bias;
  uint32_t wide_fraction =
      narrow_fraction << (WideType::FractionWidth - NarrowType::FractionWidth);

  if (narrow_exponent == ((1 << NarrowType::ExponentWidth) - 1)) {
    // Infinity or NaN
    wide_exponent = ((1 << WideType::ExponentWidth) - 1);
    // Narrow fraction is kept/widened!
  } else if (narrow_exponent == 0) {
    if (narrow_fraction == 0) {
      // +-Zero
      wide_exponent = 0;
      wide_fraction = 0;
    } else {
      // Subnormal numbers
      // We can represent it as a normalized value in wider type,
      // we have to shift fraction until we get 1.new_fraction
      // and decrement exponent for each shift.
      // FIXME; what is the implicit precondition here?
      wide_exponent = 1 - NarrowType::Bias + WideType::Bias;
      while (!(wide_fraction & (1 << (WideType::FractionWidth)))) {
        wide_exponent -= 1;
        wide_fraction <<= 1;
      }
      wide_fraction &= ((1 << WideType::FractionWidth) - 1);
    }
  }
  return (sign << WideType::SignBitPos) |
         (wide_exponent << WideType::ExponentPos) | wide_fraction;
}

// Expand IEEE-754-2008 binary16 into float32
inline uint32_t fp16ToFloat(uint16_t fp16) {
  return extendBinaryFloatingPoint<ieee_754_2008::Binary16,
                                   ieee_754_2008::Binary32>(fp16);
}

// Expand IEEE-754-2008 binary24 into float32
inline uint32_t fp24ToFloat(uint32_t fp24) {
  return extendBinaryFloatingPoint<ieee_754_2008::Binary24,
                                   ieee_754_2008::Binary32>(fp24);
}

} // namespace rawspeed
