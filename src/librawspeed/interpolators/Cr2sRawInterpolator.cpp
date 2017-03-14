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
#include "common/Common.h"                 // for ushort16, clampBits
#include "common/Point.h"                  // for iPoint2D
#include "common/RawImage.h"               // for RawImage, RawImageData
#include "decoders/RawDecoderException.h"  // for RawDecoderException (ptr o...
#include <array>                           // for array
#include <cassert>                         // for assert

using namespace std;

namespace RawSpeed {

struct Cr2sRawInterpolator::YCbCr final {
  int Y;
  int Cb;
  int Cr;

  inline static void LoadY(YCbCr* dst, const YCbCr& src) {
    assert(dst);

    dst->Y = src.Y;
  }

  inline static void LoadY(YCbCr* p, const ushort16* data) {
    assert(p);
    assert(data);

    p->Y = data[0];
  }

  inline static void LoadCbCr(YCbCr* p, const ushort16* data) {
    assert(p);
    assert(data);

    p->Cb = data[1];
    p->Cr = data[2];
  }

  inline static void Load(YCbCr* p, const ushort16* data) {
    assert(p);
    assert(data);

    LoadY(p, data);
    LoadCbCr(p, data);
  }

  YCbCr() = default;

  explicit YCbCr(ushort16* data) {
    assert(data);

    Load(this, data);
  }

  inline void signExtend() {
    Cb -= 16384;
    Cr -= 16384;
  }

  inline void applyHue(int hue) {
    Cb += hue;
    Cr += hue;
  }

  inline void process(int hue) {
    signExtend();
    applyHue(hue);
  }

  inline void interpolate(const YCbCr& p0, const YCbCr& p2) {
    // Y is already good, need to interpolate Cb and Cr
    // FIXME: dcraw does +1 before >> 1
    Cb = (p0.Cb + p2.Cb) >> 1;
    Cr = (p0.Cr + p2.Cr) >> 1;
  }

  inline void interpolate(const array<YCbCr, 2>& px) {
    interpolate(px[0], px[1]);
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
inline void Cr2sRawInterpolator::interpolate_422_row(ushort16* data, int hue,
                                                     int hue_last, int w) {
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

  // poor man's circular buffer
  bool sel = false;
  array<YCbCr, 2> px;

  // prefetch and prepare first good pixel
  YCbCr::Load(&px[sel], data);
  px[sel].process(hue);

  int x;
  for (x = 0; x < w - 2; x += 2) {
    assert(x + 4 <= w);
    assert(x % 2 == 0);

    // output first pixel, which is full
    YUV_TO_RGB<version>(px[sel], data);
    data += 3;

    // load Y from second pixel, Cb/Cr need to be interpolated
    YCbCr p;
    YCbCr::LoadY(&p, data);

    // switch to next cell in cicular buffer
    sel = !sel;

    // load third pixel, which is full, process
    YCbCr::Load(&px[sel], data + 3);
    px[sel].process(hue);

    // and finally, interpolate and output the middle pixel
    p.interpolate(px);
    YUV_TO_RGB<version>(p, data);
    data += 3;
  }

  assert(x + 2 == w);
  assert(x % 2 == 0);

  // Last two pixels, the format is:
  //      p0             p1
  //  .. [ Y1 Cb  Cr  ] [ Y2 ... ... ]

  // load, process and output first pixel, which is full
  YCbCr p(data);
  p.process(hue_last);
  YUV_TO_RGB<version>(p, data);
  data += 3;

  // load Y from second pixel, keep Cb/Cr from previous pixel, and output
  YCbCr::LoadY(&p, data);
  YUV_TO_RGB<version>(p, data);
  data += 3;
}

template <int version>
inline void Cr2sRawInterpolator::interpolate_422(int hue, int hue_last, int w,
                                                 int h) {
  assert(w > 0);
  assert(h > 0);

  for (int y = 0; y < h; y++) {
    auto data = (ushort16*)mRaw->getData(0, y);

    interpolate_422_row<version>(data, hue, hue_last, w);
  }
}

/* sRaw interpolators - ugly as sin, but does the job in reasonably speed */

// NOTE: Not thread safe, since it writes inplace.
template <int version>
inline void Cr2sRawInterpolator::interpolate_420(int hue, int w, int h) {
  assert(w >= 2);
  assert(w % 2 == 0);

  assert(h >= 2);
  assert(h % 2 == 0);

  // Current line
  ushort16* c_line;
  // Next line
  ushort16* n_line;
  // Next line again
  ushort16* nn_line;

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

  int y;
  for (y = 0; y < h - 2; y += 2) {
    assert(y + 4 <= h);
    assert(y % 2 == 0);

    c_line = (ushort16*)mRaw->getData(0, y);
    n_line = (ushort16*)mRaw->getData(0, y + 1);
    nn_line = (ushort16*)mRaw->getData(0, y + 2);

    assert(c_line);
    assert(n_line);
    assert(nn_line);

    int x;
    for (x = 0; x < w - 2; x += 2) {
      assert(x + 4 <= w);
      assert(x % 2 == 0);

      YCbCr p0(c_line);
      p0.process(hue);
      YUV_TO_RGB<version>(p0, c_line);
      c_line += 3;

      YCbCr::LoadY(&p0, c_line);

      YCbCr p1;
      YCbCr::LoadCbCr(&p1, c_line + 3);
      p1.process(hue);

      YCbCr p2;
      YCbCr::LoadY(&p2, p0);
      p2.interpolate(p0, p1);
      YUV_TO_RGB<version>(p2, c_line);
      c_line += 3;

      // Next line
      YCbCr p3;
      YCbCr::LoadY(&p3, n_line);
      YCbCr::LoadCbCr(&p3, nn_line);
      p3.process(hue);
      p3.interpolate(p0, p3);
      YUV_TO_RGB<version>(p3, n_line);
      n_line += 3;
      nn_line += 6;

      YCbCr p4;
      YCbCr::LoadCbCr(&p4, nn_line);
      p4.process(hue);

      YCbCr::LoadY(&p0, n_line);
      p0.interpolate(p0, p2, p3, p4);
      YUV_TO_RGB<version>(p0, n_line);
      n_line += 3;
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

    YCbCr p0(c_line);
    p0.process(hue);
    YUV_TO_RGB<version>(p0, c_line);
    c_line += 3;

    YCbCr::LoadY(&p0, c_line);
    YUV_TO_RGB<version>(p0, c_line);
    c_line += 3;

    // Next line
    YCbCr p1;
    YCbCr::LoadY(&p1, n_line);

    YCbCr p2;
    YCbCr::LoadCbCr(&p2, nn_line);
    p2.process(hue);

    p1.interpolate(p0, p2);
    YUV_TO_RGB<version>(p1, n_line);
    n_line += 3;

    YCbCr::LoadY(&p1, n_line);
    YUV_TO_RGB<version>(p1, n_line);
    n_line += 3;
  }

  assert(y + 2 == h);
  assert(y % 2 == 0);

  c_line = (ushort16*)mRaw->getData(0, y);
  n_line = (ushort16*)mRaw->getData(0, y + 1);
  nn_line = nullptr;

  assert(c_line);
  assert(n_line);

  // Last two lines, the format is:
  //          p0             p1             p2             p3
  //           .. .   .       .. .   .       .. .   .       .. .   .
  //  row 0: [ Y1 Cb  Cr  ] [ Y2 ... ... ] [ Y1 Cb  Cr  ] [ Y2 ... ... ] ...
  //  row 1: [ Y3 ... ... ] [ Y4 ... ... ] [ Y3 ... ... ] [ Y4 ... ... ] ...

  int x;
  for (x = 0; x < w; x += 2) {
    assert(x + 2 <= w);
    // FIXME: assert(x + 4 <= w);
    assert(x % 2 == 0);

    // load, process and output first pixel of first row, which is full
    YCbCr p(c_line);
    p.process(hue);
    YUV_TO_RGB<version>(p, c_line);
    c_line += 3;

    // rest keeps Cb/Cr from this original pixel

    // load Y from second pixel of first row; and output
    YCbCr::LoadY(&p, c_line);
    YUV_TO_RGB<version>(p, c_line);
    c_line += 3;

    // load Y from first pixel of second row; and output
    YCbCr::LoadY(&p, n_line);
    YUV_TO_RGB<version>(p, n_line);
    n_line += 3;

    // load Y from second pixel of second row; and output
    YCbCr::LoadY(&p, n_line);
    YUV_TO_RGB<version>(p, n_line);
    n_line += 3;
  }

  assert(y + 2 == h);
  assert(y % 2 == 0);

  // FIXME !!!
  // assert(x + 2 == w);
  assert(x % 2 == 0);

  // Last two pixels of last two lines, the format is:
  //               p0             p1
  //                .. .   .       .. .   .
  //  row 0:  ... [ Y1 Cb  Cr  ] [ Y2 ... ... ]
  //  row 1:  ... [ Y3 ... ... ] [ Y4 ... ... ]
}

inline void Cr2sRawInterpolator::STORE_RGB(ushort16* X, int r, int g, int b) {
  assert(X);

  X[0] = clampBits(r >> 8, 16);
  X[1] = clampBits(g >> 8, 16);
  X[2] = clampBits(b >> 8, 16);
}

template </* int version */>
/* Algorithm found in EOS 40D */
inline void Cr2sRawInterpolator::YUV_TO_RGB<0>(const YCbCr& p, ushort16* X) {
  assert(X);

  int r, g, b;
  r = sraw_coeffs[0] * (p.Y + p.Cr - 512);
  g = sraw_coeffs[1] * (p.Y + ((-778 * p.Cb - (p.Cr * 2048)) >> 12) - 512);
  b = sraw_coeffs[2] * (p.Y + (p.Cb - 512));
  STORE_RGB(X, r, g, b);
}

template </* int version */>
inline void Cr2sRawInterpolator::YUV_TO_RGB<1>(const YCbCr& p, ushort16* X) {
  assert(X);

  int r, g, b;
  r = sraw_coeffs[0] * (p.Y + ((50 * p.Cb + 22929 * p.Cr) >> 12));
  g = sraw_coeffs[1] * (p.Y + ((-5640 * p.Cb - 11751 * p.Cr) >> 12));
  b = sraw_coeffs[2] * (p.Y + ((29040 * p.Cb - 101 * p.Cr) >> 12));
  STORE_RGB(X, r, g, b);
}

template </* int version */>
/* Algorithm found in EOS 5d Mk III */
inline void Cr2sRawInterpolator::YUV_TO_RGB<2>(const YCbCr& p, ushort16* X) {
  assert(X);

  int r, g, b;
  r = sraw_coeffs[0] * (p.Y + p.Cr);
  g = sraw_coeffs[1] * (p.Y + ((-778 * p.Cb - (p.Cr * 2048)) >> 12));
  b = sraw_coeffs[2] * (p.Y + p.Cb);
  STORE_RGB(X, r, g, b);
}

template </* int version */>
void Cr2sRawInterpolator::interpolate_422<0>(int w, int h) {
  auto hue = raw_hue;
  auto hue_last = 0;
  interpolate_422<0>(hue, hue_last, w, h);
}

template <int version> void Cr2sRawInterpolator::interpolate_422(int w, int h) {
  auto hue = raw_hue;
  interpolate_422<version>(hue, hue, w, h);
}

template <int version> void Cr2sRawInterpolator::interpolate_420(int w, int h) {
  auto hue = raw_hue;
  interpolate_420<version>(hue, w, h);
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

} // namespace RawSpeed
