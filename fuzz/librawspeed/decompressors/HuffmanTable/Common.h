/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2017-2018 Roman Lebedev

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

#include "io/Buffer.h"     // for Buffer
#include "io/ByteStream.h" // for ByteStream

template <typename T> static T createHuffmanTable(rawspeed::ByteStream& bs) {
  T ht;

  // first 16 bytes are consumed as n-codes-per-length
  const auto count = ht.setNCodesPerLength(bs.getBuffer(16));

  // and then count more bytes consumed as code values
  ht.setCodeValues(bs.getBuffer(count));

  // and one more byte as 'fixDNGBug16' boolean
  const auto bb = bs.getBuffer(1);
  const bool fixDNGBug16 = bb[0] != 0;
  ht.setup(FULLDECODE, fixDNGBug16);

  return ht;
}
