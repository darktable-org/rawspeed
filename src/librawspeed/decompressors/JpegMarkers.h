/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2017 Axel Waggershauser

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

#include "adt/Optional.h"
#include "io/ByteStream.h"
#include <cstdint>

#pragma once

/*
 * The following enum and two structs are stolen from the IJG JPEG library
 * Comments added by tm. See also Copyright in PrefixCodeDecoder.h.
 */

namespace rawspeed {

// JPEG marker codes
enum class JpegMarker : uint8_t {
  STUFF = 0x00,
  SOF0 = 0xc0, // baseline DCT
  SOF1 = 0xc1, // extended sequential DCT
  SOF2 = 0xc2, // progressive DCT
  SOF3 = 0xc3, // lossless (sequential)

  SOF5 = 0xc5, // differential sequential DCT

  SOF6 = 0xc6, // differential progressive DCT

  SOF7 = 0xc7, // differential lossless

  JPG = 0xc8,   // JPEG extensions
  SOF9 = 0xc9,  // extended sequential DCT
  SOF10 = 0xca, // progressive DCT
  SOF11 = 0xcb, // lossless (sequential)

  SOF13 = 0xcd, // differential sequential DCT

  SOF14 = 0xce, // differential progressive DCT

  SOF15 = 0xcf, // differential lossless

  DHT = 0xc4, // define Huffman tables

  DAC = 0xcc, // define arithmetic conditioning table

  RST0 = 0xd0, // restart
  RST1 = 0xd1, // restart
  RST2 = 0xd2, // restart
  RST3 = 0xd3, // restart
  RST4 = 0xd4, // restart
  RST5 = 0xd5, // restart
  RST6 = 0xd6, // restart
  RST7 = 0xd7, // restart

  SOI = 0xd8, // start of image
  EOI = 0xd9, // end of image
  SOS = 0xda, // start of scan
  DQT = 0xdb, // define quantization tables
  DNL = 0xdc, // define number of lines
  DRI = 0xdd, // define restart interval
  DHP = 0xde, // define hierarchical progression
  EXP = 0xdf, // expand reference image(s)

  APP0 = 0xe0,  // application marker, used for JFIF
  APP1 = 0xe1,  // application marker
  APP2 = 0xe2,  // application marker
  APP3 = 0xe3,  // application marker
  APP4 = 0xe4,  // application marker
  APP5 = 0xe5,  // application marker
  APP6 = 0xe6,  // application marker
  APP7 = 0xe7,  // application marker
  APP8 = 0xe8,  // application marker
  APP9 = 0xe9,  // application marker
  APP10 = 0xea, // application marker
  APP11 = 0xeb, // application marker
  APP12 = 0xec, // application marker
  APP13 = 0xed, // application marker
  APP14 = 0xee, // application marker, used by Adobe
  APP15 = 0xef, // application marker

  JPG0 = 0xf0, // reserved for JPEG extensions

  JPG13 = 0xfd, // reserved for JPEG extensions

  COM = 0xfe, // comment

  TEM = 0x01, // temporary use
  FILL = 0xFF

};

inline Optional<JpegMarker> peekMarker(ByteStream input) {
  uint8_t c0 = input.peekByte(0);
  uint8_t c1 = input.peekByte(1);

  if (c0 == 0xFF && c1 != 0 && c1 != 0xFF)
    return static_cast<JpegMarker>(c1);
  return {};
}

inline Optional<ByteStream> advanceToNextMarker(ByteStream input,
                                                bool skipPadding) {
  while (input.getRemainSize() >= 2) {
    if (Optional<JpegMarker> m = peekMarker(input))
      return input;

    // Marker not found. Might there be leading padding bytes?
    if (!skipPadding)
      break; // Nope, give up.

    // Advance by a single(!) byte and try again.
    input.skipBytes(1);
  }

  return std::nullopt;
}

// Get the number of this restart marker (modulo 8).
inline Optional<int> getRestartMarkerNumber(JpegMarker m) {
  switch (m) {
    using enum JpegMarker;
  case RST0:
  case RST1:
  case RST2:
  case RST3:
  case RST4:
  case RST5:
  case RST6:
  case RST7:
    return static_cast<uint8_t>(m) - static_cast<uint8_t>(RST0);
  default:
    return std::nullopt; // Not a restart marker.
  }
}

} // namespace rawspeed
