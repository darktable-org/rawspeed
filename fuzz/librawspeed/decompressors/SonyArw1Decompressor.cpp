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

#include "decompressors/SonyArw1Decompressor.h" // for SonyArw1Decompre...
#include "common/RawImage.h"                    // for RawImage
#include "common/RawspeedException.h"           // for RawspeedException
#include "io/Buffer.h"                          // for Buffer, DataBuffer
#include "io/ByteStream.h"                      // for ByteStream
#include "io/Endianness.h"                      // for Endianness, Endianness::
#include <cassert>                              // for assert
#include <cstdint>                              // for uint8_t
#include <cstdio>                               // for size_t

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size);

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size) {
  assert(Data);

  try {
    const rawspeed::Buffer b(Data, Size);
    const rawspeed::DataBuffer db(b, rawspeed::Endianness::little);
    rawspeed::ByteStream bs(db);

    const rawspeed::uint32 width = bs.getU32();
    const rawspeed::uint32 height = bs.getU32();
    const auto format = rawspeed::RawImageType(bs.getU32());
    const rawspeed::uint32 cpp = bs.getU32();

    rawspeed::RawImage mRaw(rawspeed::RawImage::create(
        rawspeed::iPoint2D(width, height), format, cpp));

    rawspeed::SonyArw1Decompressor a(mRaw);

    mRaw->createData();

    a.decompress(bs.getStream(bs.getRemainSize()));

    mRaw->checkMemIsInitialized();
  } catch (rawspeed::RawspeedException&) {
    // Exceptions are good, crashes are bad.
  }

  return 0;
}
