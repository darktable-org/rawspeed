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

#include "decompressors/SamsungV2Decompressor.h" // for SamsungV2Decompressor
#include "common/RawImage.h"                     // for RawImage, RawImageData
#include "common/RawspeedException.h"            // for RawspeedException
#include "fuzz/Common.h"                         // for CreateRawImage
#include "io/Buffer.h"                           // for Buffer, DataBuffer
#include "io/ByteStream.h"                       // for ByteStream
#include "io/Endianness.h" // for Endianness, Endianness::little
#include <cassert>         // for assert
#include <cstdint>         // for uint32_t, uint8_t
#include <cstdio>          // for size_t

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size);

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size) {
  assert(Data);

  try {
    const rawspeed::Buffer b(Data, Size);
    const rawspeed::DataBuffer db(b, rawspeed::Endianness::little);
    rawspeed::ByteStream bs(db);

    rawspeed::RawImage mRaw(CreateRawImage(bs));

    const auto bit = bs.get<uint32_t>();
    rawspeed::ByteStream rawData = bs.getStream(bs.getRemainSize());

    rawspeed::SamsungV2Decompressor p(mRaw, rawData, bit);
    mRaw->createData();
    p.decompress();

    mRaw->checkMemIsInitialized();
  } catch (const rawspeed::RawspeedException&) {
    // Exceptions are good, crashes are bad.
  }

  return 0;
}
