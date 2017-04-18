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

#ifndef PUMP
#error PUMP must be defined to one of rawspeeds pumps
#endif
#ifndef FULLDECODE
#error FULLDECODE must be defined as bool
#endif

#include "common/RawspeedException.h"   // for RawspeedException
#include "decompressors/HuffmanTable.h" // for HuffmanTable
#include "io/BitPumpJPEG.h"             // IWYU pragma: keep
#include "io/BitPumpLSB.h"              // IWYU pragma: keep
#include "io/BitPumpMSB.h"              // IWYU pragma: keep
#include "io/BitPumpMSB16.h"            // IWYU pragma: keep
#include "io/BitPumpMSB32.h"            // IWYU pragma: keep
#include "io/BitStream.h"               // for BitStream
#include "io/Buffer.h"                  // for Buffer, DataBuffer
#include "io/ByteStream.h"              // for ByteStream
#include <cassert>                      // for assert
#include <cstdint>                      // for uint8_t
#include <cstdio>                       // for size_t

rawspeed::HuffmanTable createHuffmanTable(rawspeed::ByteStream* bs);

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size);

rawspeed::HuffmanTable createHuffmanTable(rawspeed::ByteStream* bs) {
  rawspeed::HuffmanTable ht;

  // first 16 bytes are consumed as n-codes-per-length
  const auto count = ht.setNCodesPerLength(bs->getBuffer(16));

  // and then count more bytes consumed as code values
  ht.setCodeValues(bs->getBuffer(count));

  // and one more byte as 'fixDNGBug16' boolean
  const auto bb = bs->getBuffer(1);
  const bool fixDNGBug16 = bb[0] != 0;
  ht.setup(FULLDECODE, fixDNGBug16);

  return ht;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size) {
  assert(Data);

  try {
    const rawspeed::Buffer b(Data, Size);
    const rawspeed::DataBuffer db(b, true);
    rawspeed::ByteStream bs(db);

    const rawspeed::HuffmanTable ht = createHuffmanTable(&bs);

    // should have consumed 16 bytes for n-codes-per-length,
    // at *least* 1 byte as code value, and 1 byte as 'fixDNGBug16' boolean
    assert(bs.getPosition() >= 18);

    // FIXME: BitPumpJPEG timeouts
    rawspeed::PUMP bits(bs);

    while (true)
      ht.decode<decltype(bits), FULLDECODE>(bits);
  } catch (rawspeed::RawspeedException&) {
    return 0;
  }

  return 0;
}

// for i in $(seq -w 0 64); do dd if=/dev/urandom bs=1024 count=1024 of=$i; done
