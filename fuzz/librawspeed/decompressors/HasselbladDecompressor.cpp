/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2023 Roman Lebedev

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

#include "decompressors/HasselbladDecompressor.h"
#include "MemorySanitizer.h"
#include "adt/Casts.h"
#include "codes/PrefixCodeDecoder.h"
#include "codes/PrefixCodeDecoder/Common.h"
#include "common/RawImage.h"
#include "common/RawspeedException.h"
#include "fuzz/Common.h"
#include "io/Buffer.h"
#include "io/ByteStream.h"
#include "io/Endianness.h"
#include <cassert>
#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size);

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size) {
  assert(Data);

  try {
    const rawspeed::Buffer b(
        Data, rawspeed::implicit_cast<rawspeed::Buffer::size_type>(Size));
    const rawspeed::DataBuffer db(b, rawspeed::Endianness::little);
    rawspeed::ByteStream bs(db);

    rawspeed::RawImage mRaw(CreateRawImage(bs));

    const auto ht = createPrefixCodeDecoder<rawspeed::PrefixCodeDecoder<>>(bs);
    const auto initPred = bs.get<uint16_t>();

    rawspeed::HasselbladDecompressor::PerComponentRecipe rec = {ht, initPred};

    rawspeed::HasselbladDecompressor d(
        mRaw, rec,
        bs.getSubStream(/*offset=*/0).peekRemainingBuffer().getAsArray1DRef());
    mRaw->createData();
    (void)d.decompress();

    rawspeed::MSan::CheckMemIsInitialized(
        mRaw->getByteDataAsUncroppedArray2DRef());
  } catch (const rawspeed::RawspeedException&) { // NOLINT(bugprone-empty-catch)
    // Exceptions are good, crashes are bad.
  }

  return 0;
}
