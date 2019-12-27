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

#include "interpolators/Cr2sRawInterpolator.h"
#include "common/Common.h"                // for clampBits
#include "common/Point.h"                 // for iPoint2D
#include "common/RawImage.h"              // for RawImage, RawImageData
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include <array>                          // for array
#include <cassert>                        // for assert
#include <cstdint>                        // for uint16_t

using std::array;

namespace rawspeed {

struct Cr2sRawInterpolator::YCbCr final {
  int Y;
  int Cb;
  int Cr;

  inline static void LoadY(YCbCr* p, const uint16_t* data) {
    assert(p);
    assert(data);

    p->Y = data[0];
  }

  inline static void LoadCbCr(YCbCr* p, const uint16_t* data) {
    assert(p);
    assert(data);

    p->Cb = data[1];
    p->Cr = data[2];
  }

  inline static void LoadYCbCr(YCbCr* p, const uint16_t* data) {
    assert(p);
    assert(data);

    LoadY(p, data);
    LoadCbCr(p, data);
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

  inline void interpolate(const YCbCr& p0, const YCbCr& p2) {
    // Y is already good, need to interpolate Cb and Cr
    // FIXME: dcraw does +1 before >> 1
    Cb = (p0.Cb + p2.Cb) >> 1;
    Cr = (p0.Cr + p2.Cr) >> 1;
  }

  inline void interpolate(const YCbCr& p0, const YCbCr& p1, const YCbCr& p2,
                          const YCbCr& p3) {
    // Y is already good, need to interpolate Cb and Cr
    // FIXME: dcraw does +1 before >> 1
    Cb = (p0.Cb + p1.Cb + p2.Cb + p3.Cb) >> 2;
    Cr = (p0.Cr + p1.Cr + p2.Cr + p3.Cr) >> 2;
  }
};

// NOTE: Thread safe.
template <int version>
inline void Cr2sRawInterpolator::interpolate_422_row(uint16_t* data, int w) {
  assert(data);
  assert(w >= 2);
  assert(w % 2 == 0);

  // the format is:
  //   p0             p1             p2             p3
  //  [ Y1 Cb  Cr  ] [ Y2 ... ... ] [ Y1 Cb  Cr  ] [ Y2 ... ... ] ...
  // i.e. even pixels are full, odd pixels need interpolation:
  //   p0             p1             p2             p3
  //  [ Y1 Cb  Cr  ] [ Y2 Cb* Cr* ] [ Y1 Cb  Cr  ] [ Y2 Cb* Cr* ] ...
  // for last (odd) pixel of the line,  just keep Cb/Cr from previous pixel
  // see http://lclevy.free.fr/cr2/#sraw

  int x;
  for (x = 0; x < w - 2; x += 2) {
    assert(x + 4 <= w);
    assert(x % 2 == 0);

    // load, process and output first pixel, which is full
    YCbCr p0;
    YCbCr::LoadYCbCr(&p0, data);
    p0.process(hue);
    YUV_TO_RGB<version>(p0, data);
    data += 3;

    // load Y from second pixel, Cb/Cr need to be interpolated
    YCbCr p;
    YCbCr::LoadY(&p, data);

    // load third pixel, which is full, process
    YCbCr p1;
    YCbCr::LoadYCbCr(&p1, data + 3);
    p1.process(hue);

    // and finally, interpolate and output the middle pixel
    p.interpolate(p0, p1);
    YUV_TO_RGB<version>(p, data);
    data += 3;
  }

  assert(x + 2 == w);
  assert(x % 2 == 0);

  // Last two pixels, the format is:
  //      p0             p1
  //  .. [ Y1 Cb  Cr  ] [ Y2 ... ... ]

  // load, process and output first pixel, which is full
  YCbCr p;
  YCbCr::LoadYCbCr(&p, data);
  p.process(hue);
  YUV_TO_RGB<version>(p, data);
  data += 3;

  // load Y from second pixel, keep Cb/Cr from previous pixel, and output
  YCbCr::LoadY(&p, data);
  YUV_TO_RGB<version>(p, data);
  data += 3;
}

template <int version>
inline void Cr2sRawInterpolator::interpolate_422(int w, int h) {
  assert(w > 0);
  assert(h > 0);

  for (int y = 0; y < h; y++) {
    auto data = reinterpret_cast<uint16_t*>(mRaw->getData(0, y));

    interpolate_422_row<version>(data, w);
  }
}

// NOTE: Not thread safe, since it writes inplace.
template <int version>
inline void
Cr2sRawInterpolator::interpolate_420_row(std::array<uint16_t*, 3> line, int w) {
  assert(line[0]);
  assert(line[1]);
  assert(line[2]);

  // the format is:
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

  int x;
  for (x = 0; x < w - 2; x += 2) {
    assert(x + 4 <= w);
    assert(x % 2 == 0);

    // load, process and output first pixel of first row, which is full
    YCbCr p0;
    YCbCr::LoadYCbCr(&p0, line[0]);
    p0.process(hue);
    YUV_TO_RGB<version>(p0, line[0]);
    line[0] += 3;

    // load Y from second pixel of first row
    YCbCr ph;
    YCbCr::LoadY(&ph, line[0]);

    // load Cb/Cr from third pixel of first row
    YCbCr p1;
    YCbCr::LoadCbCr(&p1, line[0] + 3);
    p1.process(hue);

    // and finally, interpolate and output the middle pixel of first row
    ph.interpolate(p0, p1);
    YUV_TO_RGB<version>(ph, line[0]);
    line[0] += 3;

    // load Y from first pixel of second row
    YCbCr pv;
    YCbCr::LoadY(&pv, line[1]);

    // load Cb/Cr from first pixel of third row
    YCbCr p2;
    YCbCr::LoadCbCr(&p2, line[2]);
    p2.process(hue);

    // and finally, interpolate and output the first pixel of second row
    pv.interpolate(p0, p2);
    YUV_TO_RGB<version>(pv, line[1]);
    line[1] += 3;
    line[2] += 6;

    // load Y from second pixel of second row
    YCbCr p;
    YCbCr::LoadY(&p, line[1]);

    // load Cb/Cr from third pixel of third row
    YCbCr p3;
    YCbCr::LoadCbCr(&p3, line[2]);
    p3.process(hue);

    // and finally, interpolate and output the second pixel of second row
    // NOTE: we interpolate 4 full pixels here, located on diagonals
    // dcraw interpolates from already interpolated pixels
    p.interpolate(p0, p1, p2, p3);
    YUV_TO_RGB<version>(p, line[1]);
    line[1] += 3;
  }

  assert(x + 2 == w);
  assert(x % 2 == 0);

  // Last two pixels of the lines, the format is:
  //              p0             p1
  //  row 0: ... [ Y1 Cb  Cr  ] [ Y2 ... ... ]
  //  row 1: ... [ Y3 ... ... ] [ Y4 ... ... ]
  //  row 2: ... [ Y1 Cb  Cr  ] [ Y2 ... ... ]
  //  row 3: ... [ Y3 ... ... ] [ Y4 ... ... ]
  //               .. .   .       .. .   .

  // load, process and output first pixel of first row, which is full
  YCbCr p0;
  YCbCr::LoadYCbCr(&p0, line[0]);
  p0.process(hue);
  YUV_TO_RGB<version>(p0, line[0]);
  line[0] += 3;

  // keep Cb/Cr from first pixel of first row
  // load Y from second pixel of first row, output
  YCbCr::LoadY(&p0, line[0]);
  YUV_TO_RGB<version>(p0, line[0]);
  line[0] += 3;

  // load Y from first pixel of second row
  YCbCr pv;
  YCbCr::LoadY(&pv, line[1]);

  // load Cb/Cr from first pixel of third row
  YCbCr p2;
  YCbCr::LoadCbCr(&p2, line[2]);
  p2.process(hue);

  // and finally, interpolate and output the first pixel of second row
  pv.interpolate(p0, p2);
  YUV_TO_RGB<version>(pv, line[1]);
  line[1] += 3;

  // keep Cb/Cr from first pixel of second row
  // load Y from second pixel of second row, output
  YCbCr::LoadY(&pv, line[1]);
  YUV_TO_RGB<version>(pv, line[1]);
  line[1] += 3;
}

// NOTE: Not thread safe, since it writes inplace.
template <int version>
inline void Cr2sRawInterpolator::interpolate_420(int w, int h) {
  assert(w >= 2);
  assert(w % 2 == 0);

  assert(h >= 2);
  assert(h % 2 == 0);

  array<uint16_t*, 3> line;

  int y;
  for (y = 0; y < h - 2; y += 2) {
    assert(y + 4 <= h);
    assert(y % 2 == 0);

    line[0] = reinterpret_cast<uint16_t*>(mRaw->getData(0, y));
    line[1] = reinterpret_cast<uint16_t*>(mRaw->getData(0, y + 1));
    line[2] = reinterpret_cast<uint16_t*>(mRaw->getData(0, y + 2));

    interpolate_420_row<version>(line, w);
  }

  assert(y + 2 == h);
  assert(y % 2 == 0);

  line[0] = reinterpret_cast<uint16_t*>(mRaw->getData(0, y));
  line[1] = reinterpret_cast<uint16_t*>(mRaw->getData(0, y + 1));
  line[2] = nullptr;

  assert(line[0]);
  assert(line[1]);
  assert(line[2] == nullptr);

  // Last two lines, the format is:
  //          p0             p1             p2             p3
  //           .. .   .       .. .   .       .. .   .       .. .   .
  //  row 0: [ Y1 Cb  Cr  ] [ Y2 ... ... ] [ Y1 Cb  Cr  ] [ Y2 ... ... ] ...
  //  row 1: [ Y3 ... ... ] [ Y4 ... ... ] [ Y3 ... ... ] [ Y4 ... ... ] ...

  int x;
  for (x = 0; x < w - 2; x += 2) {
    assert(x + 4 <= w);
    assert(x % 2 == 0);

    // load, process and output first pixel of first row, which is full
    YCbCr p0;
    YCbCr::LoadYCbCr(&p0, line[0]);
    p0.process(hue);
    YUV_TO_RGB<version>(p0, line[0]);
    line[0] += 3;

    // load Y from second pixel of first row
    YCbCr ph;
    YCbCr::LoadY(&ph, line[0]);

    // load Cb/Cr from third pixel of first row
    YCbCr p1;
    YCbCr::LoadCbCr(&p1, line[0] + 3);
    p1.process(hue);

    // and finally, interpolate and output the middle pixel of first row
    ph.interpolate(p0, p1);
    YUV_TO_RGB<version>(ph, line[0]);
    line[0] += 3;

    // keep Cb/Cr from first pixel of first row
    // load Y from first pixel of second row; and output
    YCbCr::LoadY(&p0, line[1]);
    YUV_TO_RGB<version>(p0, line[1]);
    line[1] += 3;

    // keep Cb/Cr from second pixel of first row
    // load Y from second pixel of second row; and output
    YCbCr::LoadY(&ph, line[1]);
    YUV_TO_RGB<version>(ph, line[1]);
    line[1] += 3;
  }

  assert(line[0]);
  assert(line[1]);
  assert(line[2] == nullptr);

  assert(y + 2 == h);
  assert(y % 2 == 0);

  assert(x + 2 == w);
  assert(x % 2 == 0);

  // Last two pixels of last two lines, the format is:
  //               p0             p1
  //                .. .   .       .. .   .
  //  row 0:  ... [ Y1 Cb  Cr  ] [ Y2 ... ... ]
  //  row 1:  ... [ Y3 ... ... ] [ Y4 ... ... ]

  // load, process and output first pixel of first row, which is full
  YCbCr p;
  YCbCr::LoadYCbCr(&p, line[0]);
  p.process(hue);
  YUV_TO_RGB<version>(p, line[0]);
  line[0] += 3;

  // rest keeps Cb/Cr from this original pixel, because rest only have Y

  // load Y from second pixel of first row, and output
  YCbCr::LoadY(&p, line[0]);
  YUV_TO_RGB<version>(p, line[0]);
  line[0] += 3;

  // load Y from first pixel of second row, and output
  YCbCr::LoadY(&p, line[1]);
  YUV_TO_RGB<version>(p, line[1]);
  line[1] += 3;

  // load Y from second pixel of second row, and output
  YCbCr::LoadY(&p, line[1]);
  YUV_TO_RGB<version>(p, line[1]);
  line[1] += 3;
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
    int width = mRaw->dim.x;
    int height = mRaw->dim.y;

    switch (version) {
    case 0:
      interpolate_422<0>(width, height);
      break;
    case 1:
      interpolate_422<1>(width, height);
      break;
    case 2:
      interpolate_422<2>(width, height);
      break;
    default:
      __builtin_unreachable();
    }
  } else if (subSampling.y == 2 && subSampling.x == 2) {
    int width = mRaw->dim.x;
    int height = mRaw->dim.y;

    switch (version) {
    // no known sraws with "version 0"
    case 1:
      interpolate_420<1>(width, height);
      break;
    case 2:
      interpolate_420<2>(width, height);
      break;
    default:
      __builtin_unreachable();
    }
  } else
    ThrowRDE("Unknown subsampling: (%i; %i)", subSampling.x, subSampling.y);
}

} // namespace rawspeed
