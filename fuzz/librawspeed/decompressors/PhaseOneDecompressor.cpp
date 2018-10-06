/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2018 Roman Lebedev

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

#include "decompressors/PhaseOneDecompressor.h" // for PhaseOneDecompressor
#include "common/RawImage.h"                    // for RawImage
#include "common/RawspeedException.h"           // for RawspeedException
#include "fuzz/Common.h"                        // for CreateRawImage
#include "io/Buffer.h"                          // for Buffer, DataBuffer
#include "io/ByteStream.h"                      // for ByteStream
#include "io/Endianness.h"                      // for Endianness, Endianness::
#include <algorithm>                            // for generate_n
#include <cassert>                              // for assert
#include <cstdint>                              // for uint8_t
#include <cstdio>                               // for size_t
#include <iterator>                             // for back_insert_iterator

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size);

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size) {
  assert(Data);

  try {
    const rawspeed::Buffer b(Data, Size);
    const rawspeed::DataBuffer db(b, rawspeed::Endianness::little);
    rawspeed::ByteStream bs(db);

    rawspeed::RawImage mRaw(CreateRawImage(&bs));

    const auto numStrips = bs.getU32();
    std::vector<rawspeed::PhaseOneStrip> strips;
    std::generate_n(std::back_inserter(strips), numStrips,
                    [&bs]() -> rawspeed::PhaseOneStrip {
                      const int n = bs.getU32();
                      const auto stripLen = bs.getU32();
                      return {n, bs.getStream(stripLen)};
                    });
    assert(strips.size() == numStrips);

    rawspeed::PhaseOneDecompressor f(mRaw, std::move(strips));
    mRaw->createData();
    f.decompress();

    mRaw->checkMemIsInitialized();
  } catch (rawspeed::RawspeedException&) {
    // Exceptions are good, crashes are bad.
  }

  return 0;
}
