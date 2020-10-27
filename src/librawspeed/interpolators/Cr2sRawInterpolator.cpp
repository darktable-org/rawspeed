/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2015-2017 Roman Lebedev

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

#include "rawspeedconfig.h" // for HAVE_OPENMP
#include "interpolators/Cr2sRawInterpolator.h"
#include "common/Array2DRef.h"            // for Array2DRef
#include "common/Common.h"                // for clampBits
#include "common/Point.h"                 // for iPoint2D
#include "common/RawImage.h"              // for RawImage, RawImageData
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include <array>                          // for array
#include <cassert>                        // for assert
#include <cstdint>                        // for uint16_t

namespace rawspeed {

struct Cr2sRawInterpolator::YCbCr final {
  int Y = 0;
  int Cb = 0;
  int Cr = 0;

  inline static void LoadY(YCbCr* p, const uint16_t* data) {
    assert(p);
    assert(data);

    p->Y = data[0];
  }

  inline static void LoadCbCr(YCbCr* p, const uint16_t* data) {
    assert(p);
    assert(data);

    p->Cb = data[0];
    p->Cr = data[1];
  }

  inline static void CopyCbCr(YCbCr* p, const YCbCr& pSrc) {
    assert(p);

    p->Cb = pSrc.Cb;
    p->Cr = pSrc.Cr;
  }

  YCbCr() = default;

  inline void signExtend() {
    Cb -= 16384;
    Cr -= 16384;
  }

  inline void applyHue(int hue_) {
    Cb += hue_;
    Cr += hue_;
  }

  inline void process(int hue_) {
    signExtend();
    applyHue(hue_);
  }

  inline void interpolateCbCr(const YCbCr& p0, const YCbCr& p2) {
    // Y is already good, need to interpolate Cb and Cr
    // FIXME: dcraw does +1 before >> 1
    Cb = (p0.Cb + p2.Cb) >> 1;
    Cr = (p0.Cr + p2.Cr) >> 1;
  }

  inline void interpolateCbCr(const YCbCr& p0, const YCbCr& p1, const YCbCr& p2,
                              const YCbCr& p3) {
    // Y is already good, need to interpolate Cb and Cr
    // FIXME: dcraw does +1 before >> 1
    Cb = (p0.Cb + p1.Cb + p2.Cb + p3.Cb) >> 2;
    Cr = (p0.Cr + p1.Cr + p2.Cr + p3.Cr) >> 2;
  }
};

template <int version> void Cr2sRawInterpolator::interpolate_422_row(int row) {
  const Array2DRef<uint16_t> out(mRaw->getU16DataAsUncroppedArray2DRef());

  constexpr int InputComponentsPerMCU = 4;
  constexpr int PixelsPerMCU = 2;
  constexpr int YsPerMCU = PixelsPerMCU;
  constexpr int ComponentsPerPixel = 3;
  constexpr int OutputComponentsPerMCU = ComponentsPerPixel * PixelsPerMCU;

  assert(input.width % InputComponentsPerMCU == 0);
  int numMCUs = input.width / InputComponentsPerMCU;
  assert(numMCUs > 1);

  using MCUTy = std::array<YCbCr, PixelsPerMCU>;

  auto LoadMCU = [&](int MCUIdx) {
    MCUTy MCU;
    for (int YIdx = 0; YIdx < PixelsPerMCU; ++YIdx)
      YCbCr::LoadY(&MCU[YIdx],
                   &input(row, InputComponentsPerMCU * MCUIdx + YIdx));
    YCbCr::LoadCbCr(&MCU[0],
                    &input(row, InputComponentsPerMCU * MCUIdx + YsPerMCU));
    return MCU;
  };
  auto StoreMCU = [&](const MCUTy& MCU, int MCUIdx) {
    for (int Pixel = 0; Pixel < PixelsPerMCU; ++Pixel) {
      YUV_TO_RGB<version>(MCU[Pixel],
                          &out(row, OutputComponentsPerMCU * MCUIdx +
                                        ComponentsPerPixel * Pixel));
    }
  };

  // The packed input format is:
  //   p0 p1 p0 p0     p2 p3 p2 p2
  //  [ Y1 Y2 Cb Cr ] [ Y1 Y2 Cb Cr ] ...
  // in unpacked form that is:
  //   p0             p1             p2             p3
  //  [ Y1 Cb  Cr  ] [ Y2 ... ... ] [ Y1 Cb  Cr  ] [ Y2 ... ... ] ...
  // i.e. even pixels are full, odd pixels need interpolation:
  //   p0             p1             p2             p3
  //  [ Y1 Cb  Cr  ] [ Y2 Cb* Cr* ] [ Y1 Cb  Cr  ] [ Y2 Cb* Cr* ] ...
  // for last (odd) pixel of the line,  just keep Cb/Cr from previous pixel
  // see http://lclevy.free.fr/cr2/#sraw

  int MCUIdx;
  // Process all MCU's except the last one.
  for (MCUIdx = 0; MCUIdx < numMCUs - 1; ++MCUIdx) {
    assert(MCUIdx + 1 <= numMCUs);

    // For 4:2:2, one MCU encodes 2 pixels, and odd pixels need interpolation,
    // so we need to load three pixels, and thus we must load 2 MCU's.
    std::array<MCUTy, 2> MCUs;
    for (size_t SubMCUIdx = 0; SubMCUIdx < MCUs.size(); ++SubMCUIdx)
      MCUs[SubMCUIdx] = LoadMCU(MCUIdx + SubMCUIdx);

    // Process first pixel, which is full
    MCUs[0][0].process(hue);
    // Process third pixel, which is, again, full
    MCUs[1][0].process(hue);
    // Interpolate the middle pixel, for which only the Y was known.
    MCUs[0][1].interpolateCbCr(MCUs[0][0], MCUs[1][0]);

    // And finally, store the first MCU, i.e. first two pixels.
    StoreMCU(MCUs[0], MCUIdx);
  }

  assert(MCUIdx + 1 == numMCUs);

  // Last two pixels, the packed input format is:
  //      p0 p1 p0 p0
  //  .. [ Y1 Y2 Cb Cr ]
  // in unpacked form that is:
  //      p0             p1
  //  .. [ Y1 Cb  Cr  ] [ Y2 ... ... ]

  MCUTy MCU = LoadMCU(MCUIdx);

  MCU[0].process(hue);
  YCbCr::CopyCbCr(&MCU[1], MCU[0]);

  StoreMCU(MCU, MCUIdx);
}

template <int version> void Cr2sRawInterpolator::interpolate_422() {
  const Array2DRef<uint16_t> out(mRaw->getU16DataAsUncroppedArray2DRef());
  assert(out.width > 0);
  assert(out.height > 0);

  // Benchmarking suggests that for real-world usage, it is not beneficial to
  // parallelize this, and in fact leads to worse performance.
  for (int row = 0; row < out.height; row++)
    interpolate_422_row<version>(row);
}

template <int version> void Cr2sRawInterpolator::interpolate_420_row(int row) {
  const Array2DRef<uint16_t> out(mRaw->getU16DataAsUncroppedArray2DRef());

  constexpr int X_S_F = 2;
  constexpr int Y_S_F = 2;
  constexpr int PixelsPerMCU = X_S_F * Y_S_F;
  constexpr int InputComponentsPerMCU = 2 + PixelsPerMCU;

  constexpr int YsPerMCU = PixelsPerMCU;
  constexpr int ComponentsPerPixel = 3;
  constexpr int OutputComponentsPerMCU = ComponentsPerPixel * PixelsPerMCU;

  assert(input.width % InputComponentsPerMCU == 0);
  int numMCUs = input.width / InputComponentsPerMCU;
  assert(numMCUs > 1);

  using MCUTy = std::array<std::array<YCbCr, X_S_F>, Y_S_F>;

  auto LoadMCU = [&](int Row, int MCUIdx) __attribute__((always_inline)) {
    MCUTy MCU;
    for (int MCURow = 0; MCURow < Y_S_F; ++MCURow) {
      for (int MCUCol = 0; MCUCol < X_S_F; ++MCUCol) {
        YCbCr::LoadY(&MCU[MCURow][MCUCol],
                     &input(Row, InputComponentsPerMCU * MCUIdx +
                                     X_S_F * MCURow + MCUCol));
      }
    }
    YCbCr::LoadCbCr(&MCU[0][0],
                    &input(Row, InputComponentsPerMCU * MCUIdx + YsPerMCU));
    return MCU;
  };
  auto StoreMCU = [&](const MCUTy& MCU, int MCUIdx, int Row)
      __attribute__((always_inline)) {
    for (int MCURow = 0; MCURow < Y_S_F; ++MCURow) {
      for (int MCUCol = 0; MCUCol < X_S_F; ++MCUCol) {
        YUV_TO_RGB<version>(
            MCU[MCURow][MCUCol],
            &out(2 * Row + MCURow, ((OutputComponentsPerMCU * MCUIdx) / Y_S_F) +
                                       ComponentsPerPixel * MCUCol));
      }
    }
  };

  assert(row + 1 <= input.height);

  // The packed input format is:
  //          p0 p1 p2 p3 p0 p0     p4 p5 p6 p7 p4 p4
  //  row 0: [ Y1 Y2 Y3 Y4 Cb Cr ] [ Y1 Y2 Y3 Y4 Cb Cr ] ...
  //  row 1: [ Y1 Y2 Y3 Y4 Cb Cr ] [ Y1 Y2 Y3 Y4 Cb Cr ] ...
  //           .. .. .. .. .  .      .. .. .. .. .  .
  // in unpacked form that is:
  //          p0             p1             p2             p3
  //  row 0: [ Y1 Cb  Cr  ] [ Y2 ... ... ] [ Y1 Cb  Cr  ] [ Y2 ... ... ] ...
  //  row 1: [ Y3 ... ... ] [ Y4 ... ... ] [ Y3 ... ... ] [ Y4 ... ... ] ...
  //  row 2: [ Y1 Cb  Cr  ] [ Y2 ... ... ] [ Y1 Cb  Cr  ] [ Y2 ... ... ] ...
  //  row 3: [ Y3 ... ... ] [ Y4 ... ... ] [ Y3 ... ... ] [ Y4 ... ... ] ...
  //           .. .   .       .. .   .       .. .   .       .. .   .
  // i.e. on even rows, even pixels are full, rest of pixels need interpolation
  // first, on even rows, odd pixels are interpolated using 422 algo (marked *)
  //          p0             p1             p2             p3
  //  row 0: [ Y1 Cb  Cr  ] [ Y2 Cb* Cr* ] [ Y1 Cb  Cr  ] [ Y2 Cb* Cr* ] ...
  //  row 1: [ Y3 ... ... ] [ Y4 ... ... ] [ Y3 ... ... ] [ Y4 ... ... ] ...
  //  row 2: [ Y1 Cb  Cr  ] [ Y2 Cb* Cr* ] [ Y1 Cb  Cr  ] [ Y2 Cb* Cr* ] ...
  //  row 3: [ Y3 ... ... ] [ Y4 ... ... ] [ Y3 ... ... ] [ Y4 ... ... ] ...
  //           .. .   .       .. .   .       .. .   .
  // then,  on odd rows, even pixels are interpolated (marked with #)
  //          p0             p1             p2             p3
  //  row 0: [ Y1 Cb  Cr  ] [ Y2 Cb* Cr* ] [ Y1 Cb  Cr  ] [ Y2 Cb* Cr* ] ...
  //  row 1: [ Y3 Cb# Cr# ] [ Y4 ... ... ] [ Y3 Cb# Cr# ] [ Y4 ... ... ] ...
  //  row 2: [ Y1 Cb  Cr  ] [ Y2 Cb* Cr* ] [ Y1 Cb  Cr  ] [ Y2 Cb* Cr* ] ...
  //  row 3: [ Y3 Cb# Cr# ] [ Y4 ... ... ] [ Y3 Cb# Cr# ] [ Y4 ... ... ] ...
  //           .. .   .       .. .   .       .. .   .
  // and finally, on odd rows, odd pixels are interpolated from * (marked $)
  //          p0             p1             p2             p3
  //  row 0: [ Y1 Cb  Cr  ] [ Y2 Cb* Cr* ] [ Y1 Cb  Cr  ] [ Y2 Cb* Cr* ] ...
  //  row 1: [ Y3 Cb# Cr# ] [ Y4 Cb$ Cr$ ] [ Y3 Cb# Cr# ] [ Y4 Cb$ Cr$ ] ...
  //  row 2: [ Y1 Cb  Cr  ] [ Y2 Cb* Cr* ] [ Y1 Cb  Cr  ] [ Y2 Cb* Cr* ] ...
  //  row 3: [ Y3 Cb# Cr# ] [ Y4 Cb$ Cr$ ] [ Y3 Cb# Cr# ] [ Y4 Cb$ Cr$ ] ...
  //           .. .   .       .. .   .       .. .   .
  // see http://lclevy.free.fr/cr2/#sraw

  int MCUIdx;
  for (MCUIdx = 0; MCUIdx < numMCUs - 1; ++MCUIdx) {
    assert(MCUIdx + 1 <= numMCUs);

    // For 4:2:0, one MCU encodes 4 pixels (2x2), and odd pixels need
    // interpolation, so we need to load eight pixels,
    // and thus we must load 4 MCU's.
    std::array<std::array<MCUTy, 2>, 2> MCUs;
    for (int Row = 0; Row < 2; ++Row)
      for (int Col = 0; Col < 2; ++Col)
        MCUs[Row][Col] = LoadMCU(row + Row, MCUIdx + Col);

    // Process first pixels of MCU's, which are full
    for (int Row = 0; Row < 2; ++Row)
      for (int Col = 0; Col < 2; ++Col)
        MCUs[Row][Col][0][0].process(hue);

    // Interpolate the middle pixel of first row.
    MCUs[0][0][0][1].interpolateCbCr(MCUs[0][0][0][0], MCUs[0][1][0][0]);

    // Interpolate the first pixel of second row.
    MCUs[0][0][1][0].interpolateCbCr(MCUs[0][0][0][0], MCUs[1][0][0][0]);

    // Interpolate the second pixel of second row.
    MCUs[0][0][1][1].interpolateCbCr(MCUs[0][0][0][0], MCUs[0][1][0][0],
                                     MCUs[1][0][0][0], MCUs[1][1][0][0]);

    // FIXME: we should instead simply interpolate odd pixels on even rows
    //        and then even pixels on odd rows, as specified in the standard.
    // for (int Row = 0; Row < 2; ++Row)
    //   MCUs[Row][0][0][1].interpolateCbCr(MCUs[Row][0][0][0],
    //                                      MCUs[Row][1][0][0]);
    // for (int Col = 0; Col < 2; ++Col)
    //   MCUs[0][0][1][Col].interpolateCbCr(MCUs[0][0][0][Col],
    //                                      MCUs[1][0][0][Col]);

    // And finally, store the first MCU, i.e. first two pixels on two rows.
    StoreMCU(MCUs[0][0], MCUIdx, row);
  }

  assert(MCUIdx + 1 == numMCUs);

  // Last two pixels of the lines, the packed input format is:
  //              p0 p1 p2 p3 p0 p0
  //  row 0: ... [ Y1 Y2 Y3 Y4 Cb Cr ]
  //  row 1: ... [ Y1 Y2 Y3 Y4 Cb Cr ]
  //               .. .. .. .. .  .
  // in unpacked form that is:
  //              p0             p1
  //  row 0: ... [ Y1 Cb  Cr  ] [ Y2 ... ... ]
  //  row 1: ... [ Y3 ... ... ] [ Y4 ... ... ]
  //  row 2: ... [ Y1 Cb  Cr  ] [ Y2 ... ... ]
  //  row 3: ... [ Y3 ... ... ] [ Y4 ... ... ]
  //               .. .   .       .. .   .

  std::array<MCUTy, 2> MCUs;
  for (int Row = 0; Row < 2; ++Row)
    MCUs[Row] = LoadMCU(row + Row, MCUIdx);

  for (int Row = 0; Row < 2; ++Row)
    MCUs[Row][0][0].process(hue);

  MCUs[0][1][0].interpolateCbCr(MCUs[0][0][0], MCUs[1][0][0]);

  for (int Row = 0; Row < 2; ++Row)
    YCbCr::CopyCbCr(&MCUs[0][Row][1], MCUs[0][Row][0]);

  StoreMCU(MCUs[0], MCUIdx, row);
}

template <int version> void Cr2sRawInterpolator::interpolate_420() {
  const Array2DRef<uint16_t> out(mRaw->getU16DataAsUncroppedArray2DRef());

  constexpr int X_S_F = 2;
  constexpr int Y_S_F = 2;
  constexpr int PixelsPerMCU = X_S_F * Y_S_F;
  constexpr int InputComponentsPerMCU = 2 + PixelsPerMCU;

  constexpr int YsPerMCU = PixelsPerMCU;
  constexpr int ComponentsPerPixel = 3;
  constexpr int OutputComponentsPerMCU = ComponentsPerPixel * PixelsPerMCU;

  assert(input.width % InputComponentsPerMCU == 0);
  int numMCUs = input.width / InputComponentsPerMCU;
  assert(numMCUs > 1);

  using MCUTy = std::array<std::array<YCbCr, X_S_F>, Y_S_F>;

  auto LoadMCU = [&](int Row, int MCUIdx) __attribute__((always_inline)) {
    MCUTy MCU;
    for (int MCURow = 0; MCURow < Y_S_F; ++MCURow) {
      for (int MCUCol = 0; MCUCol < X_S_F; ++MCUCol) {
        YCbCr::LoadY(&MCU[MCURow][MCUCol],
                     &input(Row, InputComponentsPerMCU * MCUIdx +
                                     X_S_F * MCURow + MCUCol));
      }
    }
    YCbCr::LoadCbCr(&MCU[0][0],
                    &input(Row, InputComponentsPerMCU * MCUIdx + YsPerMCU));
    return MCU;
  };
  auto StoreMCU = [&](const MCUTy& MCU, int MCUIdx, int Row)
      __attribute__((always_inline)) {
    for (int MCURow = 0; MCURow < Y_S_F; ++MCURow) {
      for (int MCUCol = 0; MCUCol < X_S_F; ++MCUCol) {
        YUV_TO_RGB<version>(
            MCU[MCURow][MCUCol],
            &out(2 * Row + MCURow, ((OutputComponentsPerMCU * MCUIdx) / Y_S_F) +
                                       ComponentsPerPixel * MCUCol));
      }
    }
  };

  int row;
#ifdef HAVE_OPENMP
#pragma omp parallel for default(none) schedule(static)                        \
    num_threads(rawspeed_get_number_of_processor_cores())                      \
        OMPFIRSTPRIVATECLAUSE(out) lastprivate(row)
#endif
  for (row = 0; row < input.height - 1; ++row)
    interpolate_420_row<version>(row);

  assert(row + 1 == input.height);

  // Last two lines, the packed input format is:
  //          p0 p1 p2 p3 p0 p0     p4 p5 p6 p7 p4 p4
  //           .. .. .. .. .  .      .. .. .. .. .  .
  //  row 0: [ Y1 Y2 Y3 Y4 Cb Cr ] [ Y1 Y2 Y3 Y4 Cb Cr ] ...
  // in unpacked form that is:
  //          p0             p1             p2             p3
  //           .. .   .       .. .   .       .. .   .       .. .   .
  //  row 0: [ Y1 Cb  Cr  ] [ Y2 ... ... ] [ Y1 Cb  Cr  ] [ Y2 ... ... ] ...
  //  row 1: [ Y3 ... ... ] [ Y4 ... ... ] [ Y3 ... ... ] [ Y4 ... ... ] ...

  int MCUIdx;
  for (MCUIdx = 0; MCUIdx < numMCUs - 1; ++MCUIdx) {
    assert(MCUIdx + 1 < numMCUs);

    // For 4:2:0, one MCU encodes 4 pixels (2x2), and odd pixels need
    // interpolation, so we need to load eight pixels,
    // and thus we must load 4 MCU's.
    std::array<std::array<MCUTy, 2>, 1> MCUs;
    for (int Row = 0; Row < 1; ++Row)
      for (int Col = 0; Col < 2; ++Col)
        MCUs[Row][Col] = LoadMCU(row + Row, MCUIdx + Col);

    // Process first pixels of MCU's, which are full
    for (int Row = 0; Row < 1; ++Row)
      for (int Col = 0; Col < 2; ++Col)
        MCUs[Row][Col][0][0].process(hue);

    // Interpolate the middle pixel of first row.
    MCUs[0][0][0][1].interpolateCbCr(MCUs[0][0][0][0], MCUs[0][1][0][0]);

    // Copy Cb/Cr to the first two pixels of second row from the two pixels
    // of first row.
    for (int Col = 0; Col < 2; ++Col)
      YCbCr::CopyCbCr(&MCUs[0][0][1][Col], MCUs[0][0][0][Col]);

    // And finally, store the first MCU, i.e. first two pixels on two rows.
    StoreMCU(MCUs[0][0], MCUIdx, row);
  }

  assert(MCUIdx + 1 == numMCUs);

  // Last two pixels of last two lines, the packed input format is:
  //              p0 p1 p2 p3 p0 p0
  //               .. .. .. .. .  .
  //  row 0: ... [ Y1 Y2 Y3 Y4 Cb Cr ]
  // in unpacked form that is:
  //               p0             p1
  //                .. .   .       .. .   .
  //  row 0:  ... [ Y1 Cb  Cr  ] [ Y2 ... ... ]
  //  row 1:  ... [ Y3 ... ... ] [ Y4 ... ... ]

  MCUTy MCU = LoadMCU(row, MCUIdx);

  MCU[0][0].process(hue);

  // Distribute the same Cb/Cr to all four pixels.
  for (int Row = 0; Row < 2; ++Row)
    for (int Col = 0; Col < 2; ++Col)
      YCbCr::CopyCbCr(&MCU[Row][Col], MCU[0][0]);

  StoreMCU(MCU, MCUIdx, row);
}

inline void Cr2sRawInterpolator::STORE_RGB(uint16_t* X, int r, int g, int b) {
  assert(X);

  X[0] = clampBits(r >> 8, 16);
  X[1] = clampBits(g >> 8, 16);
  X[2] = clampBits(b >> 8, 16);
}

template </* int version */>
/* Algorithm found in EOS 40D */
inline void Cr2sRawInterpolator::YUV_TO_RGB<0>(const YCbCr& p, uint16_t* X) {
  assert(X);

  int r = sraw_coeffs[0] * (p.Y + p.Cr - 512);
  int g = sraw_coeffs[1] * (p.Y + ((-778 * p.Cb - (p.Cr * 2048)) >> 12) - 512);
  int b = sraw_coeffs[2] * (p.Y + (p.Cb - 512));
  STORE_RGB(X, r, g, b);
}

template </* int version */>
inline void Cr2sRawInterpolator::YUV_TO_RGB<1>(const YCbCr& p, uint16_t* X) {
  assert(X);

  int r = sraw_coeffs[0] * (p.Y + ((50 * p.Cb + 22929 * p.Cr) >> 12));
  int g = sraw_coeffs[1] * (p.Y + ((-5640 * p.Cb - 11751 * p.Cr) >> 12));
  int b = sraw_coeffs[2] * (p.Y + ((29040 * p.Cb - 101 * p.Cr) >> 12));
  STORE_RGB(X, r, g, b);
}

template </* int version */>
/* Algorithm found in EOS 5d Mk III */
inline void Cr2sRawInterpolator::YUV_TO_RGB<2>(const YCbCr& p, uint16_t* X) {
  assert(X);

  int r = sraw_coeffs[0] * (p.Y + p.Cr);
  int g = sraw_coeffs[1] * (p.Y + ((-778 * p.Cb - (p.Cr * 2048)) >> 12));
  int b = sraw_coeffs[2] * (p.Y + p.Cb);
  STORE_RGB(X, r, g, b);
}

// Interpolate and convert sRaw data.
void Cr2sRawInterpolator::interpolate(int version) {
  assert(version >= 0 && version <= 2);

  const auto& subSampling = mRaw->metadata.subsampling;
  if (subSampling.y == 1 && subSampling.x == 2) {
    switch (version) {
    case 0:
      interpolate_422<0>();
      break;
    case 1:
      interpolate_422<1>();
      break;
    case 2:
      interpolate_422<2>();
      break;
    default:
      __builtin_unreachable();
    }
  } else if (subSampling.y == 2 && subSampling.x == 2) {
    switch (version) {
    // no known sraws with "version 0"
    case 1:
      interpolate_420<1>();
      break;
    case 2:
      interpolate_420<2>();
      break;
    default:
      __builtin_unreachable();
    }
  } else
    ThrowRDE("Unknown subsampling: (%i; %i)", subSampling.x, subSampling.y);
}

} // namespace rawspeed
