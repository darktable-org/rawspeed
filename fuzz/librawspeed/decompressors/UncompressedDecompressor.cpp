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

#include "decompressors/UncompressedDecompressor.h"
#include "MemorySanitizer.h" // for MSan
#include "adt/Point.h"
#include "common/Common.h"
#include "common/RawImage.h"          // for RawImage, RawImageData
#include "common/RawspeedException.h" // for ThrowException, RawspeedException
#include "fuzz/Common.h"              // for CreateRawImage
#include "io/Buffer.h"                // for Buffer, DataBuffer
#include "io/ByteStream.h"            // for ByteStream
#include "io/Endianness.h"            // for Endianness, Endianness::little
#include <cassert>                    // for assert
#include <cstddef>                    // for size_t
#include <cstdint>                    // for uint8_t

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size);

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size) {
  assert(Data);

  try {
    const rawspeed::Buffer b(Data, Size);
    const rawspeed::DataBuffer db(b, rawspeed::Endianness::little);
    rawspeed::ByteStream bs(db);

    rawspeed::RawImage mRaw(CreateRawImage(bs));

    int inputPitchBytes = bs.getI32();
    int bitPerPixel = bs.getI32();
    rawspeed::BitOrder order = [&bs]() {
      switch (int val = bs.getI32()) {
      case (int)rawspeed::BitOrder::LSB:
      case (int)rawspeed::BitOrder::MSB:
      case (int)rawspeed::BitOrder::MSB16:
      case (int)rawspeed::BitOrder::MSB32:
        return rawspeed::BitOrder(val);
      default:
        ThrowRSE("Unknown bit order: %u", val);
      }
      __builtin_unreachable();
    }();

    rawspeed::UncompressedDecompressor d(
        bs.getSubStream(/*offset=*/0), mRaw,
        rawspeed::iRectangle2D({0, 0}, mRaw->dim), inputPitchBytes, bitPerPixel,
        order);
    mRaw->createData();
    d.readUncompressedRaw();

    rawspeed::MSan::CheckMemIsInitialized(
        mRaw->getByteDataAsUncroppedArray2DRef());
  } catch (const rawspeed::RawspeedException&) {
    // Exceptions are good, crashes are bad.
  }

  return 0;
}
