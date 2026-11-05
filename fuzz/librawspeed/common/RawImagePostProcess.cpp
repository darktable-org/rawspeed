/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2026 The RawSpeed authors

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

// Option-matrix fuzzer over the RawImage post-processing stack:
// subFrame(), bad-pixel map/fixBadPixels(), scaleBlackWhite() (black areas,
// separate black levels, per-image black/white points -> scaleValues
// SSE2/plain paths), and the 16-bit lookup table machinery (setTable ->
// sixteenBitLookup -> doLookup), for both UINT16 and F32 image types.
//
// Threat model: all of these run on decoded attacker-controlled pixel data
// with attacker-influenced levels (TIFF/DNG tags carry black/white levels,
// lookup tables (LinearizationTable), bad-pixel lists and crop rectangles).
// None of it is reachable in the fuzzing environment today: the pipeline
// fuzzers call decodeRaw() only, which never invokes scaleBlackWhite() /
// sixteenBitLookup() (and fixBadPixels() is gated off with
// interpolateBadPixels=false), so RawImageDataU16.cpp (8.3%) and
// RawImageDataFloat.cpp (4.0%) sat mostly dark (coverage 2026-11-04).
//
// The harness mirrors the real callers (RawDecoder::decodeRaw,
// RafDecoder::decodeMetaDataInternal/applyCorrections, Cr2Decoder /
// DngDecoder linearization) with typed, bounded inputs.

#include "MemorySanitizer.h"
#include "adt/Casts.h"
#include "common/RawImage.h"
#include "common/RawspeedException.h"
#include "fuzz/Common.h"
#include "io/Buffer.h"
#include "io/ByteStream.h"
#include "io/Endianness.h"
#include <cassert>
#include <cstdint>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size);

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size) {
  assert(Data);

  try {
    const rawspeed::Buffer b(
        Data, rawspeed::implicit_cast<rawspeed::Buffer::size_type>(Size));
    const rawspeed::DataBuffer db(b, rawspeed::Endianness::little);
    rawspeed::ByteStream bs(db);

    // Image geometry and type.
    const rawspeed::RawImageType type =
        (bs.getU32() & 1) ? rawspeed::RawImageType::F32
                          : rawspeed::RawImageType::UINT16;
    const uint32_t cpp = 1 + bs.getU32() % 3;
    const int width = 16 + bs.getU32() % 48; // keep runs fast
    const int height = 16 + bs.getU32() % 48;

    rawspeed::RawImage mRaw = rawspeed::RawImage::create(
        rawspeed::iPoint2D(width, height), type, cpp);
    // NOTE: RawImage::create(dim, ...) already allocates the pixel buffer.
    mRaw->isCFA = bs.getU32() & 1;
    mRaw->cfa = CreateCFA(bs);

    // Attacker-controlled pixel data.
    for (int row = 0; row < height; ++row) {
      for (int col = 0; col < width * static_cast<int>(cpp); ++col) {
        if (type == rawspeed::RawImageType::UINT16) {
          mRaw->getU16DataAsUncroppedArray2DRef()(row, col) = bs.getU16();
        } else {
          static_assert(sizeof(float) == sizeof(uint32_t));
          const uint32_t bits = bs.getU32();
          float f;
          __builtin_memcpy(&f, &bits, sizeof(f));
          mRaw->getF32DataAsUncroppedArray2DRef()(row, col) = f;
        }
      }
    }

    // Optional crop, as the decoders' applyCorrections() would do.
    if (bs.getU32() & 1) {
      const int cropX = bs.getU32() % 8;
      const int cropY = bs.getU32() % 8;
      const int cropW = 1 + bs.getU32() % (width - cropX);
      const int cropH = 1 + bs.getU32() % (height - cropY);
      mRaw->subFrame(rawspeed::iRectangle2D(rawspeed::iPoint2D(cropX, cropY),
                                            rawspeed::iPoint2D(cropW, cropH)));
    }

    // Bad pixels: positions plus optionally the accelerator map, exactly the
    // format decoders feed (x | (y << 16)).
    const bool haveBadPixels = bs.getU32() & 1;
    if (haveBadPixels) {
      const int count = bs.getU32() % 17;
      for (int i = 0; i < count; ++i) {
        const uint32_t x = bs.getU32() % width;
        const uint32_t y = bs.getU32() % height;
        mRaw->mBadPixelPositions.push_back(x | (y << 16));
      }
      if (bs.getU32() & 1)
        mRaw->createBadPixelMap();
    }

    // Black/white point configuration.
    mRaw->blackLevel = static_cast<int>(bs.getU32() % 65536);
    if (bs.getU32() & 1)
      mRaw->whitePoint = static_cast<int>(bs.getU32() % 65536);
    // Real callers (DngDecoder for float DNGs) always provide the white point
    // for F32 images: "lack of whitePoint means that it is pre-normalized"
    // (RawImageDataFloat.cpp), i.e. the white level of 1.
    if (type == rawspeed::RawImageType::F32 && !mRaw->whitePoint)
      mRaw->whitePoint = 1;
    if (bs.getU32() & 1) {
      for (int& i : mRaw->blackLevelSeparateStorage)
        i = static_cast<int>(bs.getU32() % 65536);
      mRaw->blackLevelSeparate = rawspeed::Array2DRef<int>(
          mRaw->blackLevelSeparateStorage.data(), 2, 2);
    }
    const int blackAreaCount = bs.getU32() % 3;
    for (int i = 0; i < blackAreaCount; ++i) {
      const bool isVertical = bs.getU32() & 1;
      const int bound = isVertical ? width : height;
      const uint32_t offset = bs.getU32() % (bound + 1);
      const uint32_t size = bs.getU32() % (bound + 1);
      mRaw->blackAreas.emplace_back(offset, size, isVertical);
    }

    // Linearization table (UINT16 only; the F32 doLookup rejects by design).
    const bool haveTable = bs.getU32() & 1;
    if (haveTable && type == rawspeed::RawImageType::UINT16) {
      const bool dither = bs.getU32() & 1;
      const int len = 1 + bs.getU32() % 64;
      std::vector<uint16_t> table;
      table.reserve(len);
      for (int i = 0; i < len; ++i)
        table.push_back(bs.getU16());
      mRaw->setTable(table, dither);
    }

    // The production pipeline order: fix bad pixels (decodeRaw), then the
    // application-side scaling and linearization.
    if (haveBadPixels)
      mRaw->fixBadPixels();
    mRaw->scaleBlackWhite();
    if (haveTable && type == rawspeed::RawImageType::UINT16)
      mRaw->sixteenBitLookup();

    rawspeed::MSan::CheckMemIsInitialized(
        mRaw->getByteDataAsUncroppedArray2DRef());
  } catch (const rawspeed::RawspeedException&) { // NOLINT(bugprone-empty-catch)
    // Exceptions are good, crashes are bad.
  }

  return 0;
}

