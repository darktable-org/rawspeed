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

#include "decompressors/Cr2Decompressor.h" // for Cr2Decompressor
#include "common/RawImage.h"               // for RawImage
#include "common/RawspeedException.h"      // for RawspeedException
#include "fuzz/Common.h"                   // for CreateRawImage
#include "io/Buffer.h"                     // for Buffer, DataBuffer
#include "io/ByteStream.h"                 // for ByteStream
#include "io/Endianness.h"                 // for Endianness, Endianness::
#include <algorithm>                       // for generate_n, fill_n
#include <cassert>                         // for assert
#include <cstdint>                         // for uint8_t
#include <cstdio>                          // for size_t
#include <iterator>                        // for back_insert_iterator
#include <vector>                          // for vector

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size);

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size) {
  assert(Data);

  try {
    const rawspeed::Buffer b(Data, Size);
    const rawspeed::DataBuffer db(b, rawspeed::Endianness::little);
    rawspeed::ByteStream bs(db);

    rawspeed::RawImage mRaw(CreateRawImage(&bs));

    using slice_type = int;
    std::vector<slice_type> slicesWidths;
    const unsigned sliceCount = bs.getU32();
    bs.check(sliceCount, sizeof(slice_type));
    slicesWidths.reserve(sliceCount);
    std::generate_n(
        std::back_insert_iterator<std::vector<slice_type>>(slicesWidths),
        sliceCount, [&bs]() -> slice_type { return bs.get<slice_type>(); });

    rawspeed::Cr2Decompressor c(bs, mRaw);
    mRaw->createData();
    c.decode(std::move(slicesWidths));

    mRaw->checkMemIsInitialized();
  } catch (rawspeed::RawspeedException&) {
    // Exceptions are good, crashes are bad.
  }

  return 0;
}
