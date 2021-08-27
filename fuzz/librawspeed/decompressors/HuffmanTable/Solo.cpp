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

#ifndef IMPL
#error IMPL must be defined to one of rawspeeds pumps
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
    rawspeed::ByteStream bs(db);

    const auto ht = createHuffmanTable<rawspeed::IMPL>(&bs);

    // should have consumed 16 bytes for n-codes-per-length,
    // at *least* 1 byte as code value, and 1 byte as 'fixDNGBug16' boolean
    assert(bs.getPosition() >= 18);

    rawspeed::PUMP bits(bs);

    while (true)
      ht.decode<decltype(bits), FULLDECODE>(bits);
  } catch (rawspeed::RawspeedException&) {
    return 0;
  }

  __builtin_unreachable();
}

// for i in $(seq -w 0 64); do dd if=/dev/urandom bs=1024 count=1024 of=$i; done
