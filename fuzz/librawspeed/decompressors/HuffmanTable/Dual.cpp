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

#ifndef IMPL0
#error IMPL0 must be defined to one of rawspeeds pumps
#endif
#ifndef IMPL1
#error IMPL1 must be defined to one of rawspeeds pumps
#endif
#ifndef PUMP
#error PUMP must be defined to one of rawspeeds pumps
#endif
#ifndef FULLDECODE
#error FULLDECODE must be defined as bool
#endif

#include "common/RawspeedException.h"          // for RawspeedException
#include "decompressors/HuffmanTable.h"        // IWYU pragma: keep
#include "decompressors/HuffmanTable/Common.h" // for createHuffmanTable
#include "decompressors/HuffmanTableLUT.h"     // IWYU pragma: keep
#include "decompressors/HuffmanTableLookup.h"  // IWYU pragma: keep
#include "decompressors/HuffmanTableTree.h"    // IWYU pragma: keep
#include "decompressors/HuffmanTableVector.h"  // IWYU pragma: keep
#include "io/BitPumpJPEG.h"                    // IWYU pragma: keep
#include "io/BitPumpMSB.h"                     // IWYU pragma: keep
#include "io/BitPumpMSB32.h"                   // IWYU pragma: keep
#include "io/BitStream.h"                      // for BitStream
#include "io/Buffer.h"                         // for Buffer, DataBuffer
#include "io/ByteStream.h"                     // for ByteStream
#include "io/Endianness.h"                     // for Endianness, Endiannes...
#include "io/IOException.h"                    // for IOException
#include <cassert>                             // for assert
#include <cstdint>                             // for uint8_t
#include <cstdio>                              // for size_t
#include <initializer_list>                    // IWYU pragma: keep

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size);

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size) {
  assert(Data);

  try {
    const rawspeed::Buffer b(Data, Size);
    const rawspeed::DataBuffer db(b, rawspeed::Endianness::little);

    rawspeed::ByteStream bs0(db);
    rawspeed::ByteStream bs1(db);

    bool failure0 = false;
    bool failure1 = false;

    rawspeed::IMPL0 ht0;
    rawspeed::IMPL1 ht1;

    try {
      ht0 = createHuffmanTable<rawspeed::IMPL0>(&bs0);
    } catch (rawspeed::RawspeedException&) {
      failure0 = true;
    }
    try {
      ht1 = createHuffmanTable<rawspeed::IMPL1>(&bs1);
    } catch (rawspeed::RawspeedException&) {
      failure1 = true;
    }

    // They both should either fail or succeed, else there is a bug.
    assert(failure0 == failure1);

    // If any failed, we can't continue.
    if (failure0 || failure1)
      return 0;

    // should have consumed 16 bytes for n-codes-per-length,
    // at *least* 1 byte as code value, and 1 byte as 'fixDNGBug16' boolean
    assert(bs0.getPosition() == bs1.getPosition());
    assert(bs0.getPosition() >= 18);

    rawspeed::PUMP bits0(bs0);
    rawspeed::PUMP bits1(bs1);

    while (true) {
      int decoded0, decoded1;

      try {
        decoded0 = ht0.decode<decltype(bits0), FULLDECODE>(bits0);
      } catch (rawspeed::IOException&) {
        // For now, let's ignore stream depleteon issues.
        throw;
      } catch (rawspeed::RawspeedException&) {
        failure0 = true;
      }
      try {
        decoded1 = ht1.decode<decltype(bits1), FULLDECODE>(bits1);
      } catch (rawspeed::IOException&) {
        // For now, let's ignore stream depleteon issues.
        throw;
      } catch (rawspeed::RawspeedException&) {
        failure1 = true;
      }

      // They both should either fail or succeed, else there is a bug.
      assert(failure0 == failure1);

      // If any failed, we can't continue.
      if (failure0 || failure1)
        return 0;

      (void)decoded0;
      (void)decoded1;

      // They both should have decoded the same value.
      assert(decoded0 == decoded1);
    }
  } catch (rawspeed::RawspeedException&) {
    return 0;
  }

  __builtin_unreachable();
}
