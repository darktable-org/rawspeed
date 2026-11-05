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

// Component-API fuzzer for the Canon sRaw (YCbCr -> RGB) interpolation stage.
//
// Threat model: an attacker-crafted CR2 selects the sRaw pipeline, and the
// sRaw coefficients are read verbatim from the CANONCOLORDATA tag (three
// full-range uint16 values), while pixel data comes from the attacker-packed
// lossless JPEG. The interpolation arithmetic (fixed-point YUV_TO_RGB with
// large literal coefficients, clampBits on store) is therefore driven by
// attacker-controlled data. The production corpus cannot reach this code at
// all: sRawInterpolate() requires camera metadata (checkCameraSupported ->
// getCamera()), which the fuzzing environment does not have, so
// Cr2sRawInterpolator.cpp sat at 0/361 lines (coverage 2026-11-04).
//
// The harness mirrors the real call site (Cr2Decoder::sRawInterpolate):
// typed inputs (algorithm version 0/1/2, 4:2:2 vs 4:2:0 subsampling, hue from
// the set produced by Cr2Decoder::getHue(), MCU-aligned dimensions, three
// uint16 coefficients, uint16 YCbCr pixels), and the output image metadata
// subsampling factors select the interpolation kernel exactly like upstream.

#include "interpolators/Cr2sRawInterpolator.h"
#include "MemorySanitizer.h"
#include "adt/Casts.h"
#include "adt/Point.h"
#include "common/RawImage.h"
#include "common/RawspeedException.h"
#include "io/Buffer.h"
#include "io/ByteStream.h"
#include "io/Endianness.h"
#include <array>
#include <cassert>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size);

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size) {
  assert(Data);

  try {
    const rawspeed::Buffer b(
        Data, rawspeed::implicit_cast<rawspeed::Buffer::size_type>(Size));
    const rawspeed::DataBuffer db(b, rawspeed::Endianness::little);
    rawspeed::ByteStream bs(db);

    // sRaw algorithm version: 0 (EOS 40D), 1 (default), 2 (EOS 5D Mk III).
    const int version = rawspeed::implicit_cast<int>(bs.getU32() % 3);

    // Chroma subsampling: 0 -> 4:2:2 {2,1}, 1 -> 4:2:0 {2,2}.
    const rawspeed::iPoint2D subsampling =
        (bs.getU32() & 1) ? rawspeed::iPoint2D(2, 2) : rawspeed::iPoint2D(2, 1);

    // Hue, from the exact value set produced by Cr2Decoder::getHue().
    static constexpr std::array<int, 4> hues = {0, 1, 2, 4};
    const int hue = hues[bs.getU32() % hues.size()];

    // Subsampled input dimensions. The interpolator invariants require MCU
    // alignment (4 comps/MCU for 4:2:2, 6 for 4:2:0) and strictly more than
    // one MCU (real sRAW images always span multiple MCUs); 4:2:0
    // additionally processes vertical pairs, so the height is even.
    const int mcuAlignment = (subsampling.y == 2) ? 6 : 4;
    const int width = mcuAlignment * (2 + bs.getU32() % 30); // 2..31 MCUs
    int height = 1 + bs.getU32() % 31;
    if (subsampling.y == 2)
      height = 2 * height; // even height for 4:2:0

    // sRaw reconstruction coefficients, read as full-range uint16 exactly
    // like Cr2Decoder::sRawInterpolate() reads them from CANONCOLORDATA.
    std::array<int, 3> sraw_coeffs;
    sraw_coeffs[0] = bs.getU16();
    sraw_coeffs[1] = bs.getU16();
    sraw_coeffs[2] = bs.getU16();

    // The subsampled YCbCr-packed input image, as produced by the LJpeg
    // decoder in the real sRaw pipeline. Note: RawImage::create(dim, ...)
    // already allocates the pixel buffer.
    rawspeed::RawImage in = rawspeed::RawImage::create(
        rawspeed::iPoint2D(width, height), rawspeed::RawImageType::UINT16, 1);
    auto inRef = in->getU16DataAsUncroppedArray2DRef();
    for (int row = 0; row < height; ++row) {
      for (int col = 0; col < width; ++col)
        inRef(row, col) = bs.getU16(); // IOException on EOF: fine.
    }

    // The interpolated output image; dimensions as in Cr2Decoder, and the
    // subsampling metadata selects the interpolation kernel.
    const rawspeed::iPoint2D outDims(
        subsampling.x * (width / (2 + subsampling.x * subsampling.y)),
        subsampling.y * height);
    rawspeed::RawImage out =
        rawspeed::RawImage::create(outDims, rawspeed::RawImageType::UINT16, 3);
    out->metadata.subsampling = subsampling;

    rawspeed::Cr2sRawInterpolator interp(
        out, in->getU16DataAsUncroppedArray2DRef(), sraw_coeffs, hue);
    interp.interpolate(version);

    rawspeed::MSan::CheckMemIsInitialized(
        out->getByteDataAsUncroppedArray2DRef());
  } catch (const rawspeed::RawspeedException&) { // NOLINT(bugprone-empty-catch)
    // Exceptions are good, crashes are bad.
  }

  return 0;
}
