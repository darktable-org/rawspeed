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
#include <cassert>                         // for assert

using namespace std;

namespace RawSpeed {

struct Cr2sRawInterpolator::YCbCr final {
  int Y;
  int Cb;
  int Cr;

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

  inline void interpolate(const YCbCr& p0, const YCbCr& p2) {
    // Y is already good, need to interpolate Cb and Cr
    // FIXME: dcraw does +1 before >> 1
    Cb = (p0.Cb + p2.Cb) >> 1;
    Cr = (p0.Cr + p2.Cr) >> 1;
  }
};

/* sRaw interpolators - ugly as sin, but does the job in reasonably speed */

// Note: Thread safe.

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

  int x;
  for (x = 0; x < w - 2; x += 2) {
    assert(x + 4 <= w);
    assert(x % 2 == 0);

    // load, process and output first pixel, which is full
    YCbCr p0(data);
    p0.signExtend();
    p0.applyHue(hue);
    YUV_TO_RGB<version>(p0.Y, p0.Cb, p0.Cr, data, 0);
    data += 3;

    // load Y from second pixel, Cb/Cr need to be interpolated
    YCbCr p1;
    YCbCr::LoadY(&p1, data);

    // load Cb/Cr from third pixel, process
    YCbCr p2;
    YCbCr::LoadCbCr(&p2, data + 3);
    p2.signExtend();
    p2.applyHue(hue);

    // and finally, interpolate and output the second pixel
    p1.interpolate(p0, p2);
    YUV_TO_RGB<version>(p1.Y, p1.Cb, p1.Cr, data, 0);
    data += 3;
  }

  assert(x + 2 == w);
  assert(x % 2 == 0);

  // Last two pixels, the format is:
  //      p0             p1
  //  .. [ Y1 Cb  Cr  ] [ Y2 ... ... ]

  // load, process and output first pixel, which is full
  YCbCr p(data);
  p.signExtend();
  p.applyHue(hue_last);
  YUV_TO_RGB<version>(p.Y, p.Cb, p.Cr, data, 0);
  data += 3;

  // load Y from second pixel, keep Cb/Cr from previous pixel, and output
  YCbCr::LoadY(&p, data);
  YUV_TO_RGB<version>(p.Y, p.Cb, p.Cr, data, 0);
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

// Note: Not thread safe, since it writes inplace.
template <int version>
inline void Cr2sRawInterpolator::interpolate_420(int hue, int w, int h) {
  // Last pixel should not be interpolated
  w--;

  const int end_h = h - 1;

  static constexpr const bool atLastLine = true;

  // Current line
  ushort16* c_line;
  // Next line
  ushort16* n_line;
  // Next line again
  ushort16* nn_line;

  int off;

  for (int y = 0; y < end_h; y++) {
    c_line = (ushort16*)mRaw->getData(0, y * 2);
    n_line = (ushort16*)mRaw->getData(0, y * 2 + 1);
    nn_line = (ushort16*)mRaw->getData(0, y * 2 + 2);
    off = 0;
    for (int x = 0; x < w; x++) {
      int Y = c_line[off];
      int Cb = c_line[off + 1] - hue;
      int Cr = c_line[off + 2] - hue;
      YUV_TO_RGB<version>(Y, Cb, Cr, c_line, off);

      Y = c_line[off + 3];
      int Cb2 = (Cb + c_line[off + 1 + 6] - hue) >> 1;
      int Cr2 = (Cr + c_line[off + 2 + 6] - hue) >> 1;
      YUV_TO_RGB<version>(Y, Cb2, Cr2, c_line, off + 3);

      // Next line
      Y = n_line[off];
      int Cb3 = (Cb + nn_line[off + 1] - hue) >> 1;
      int Cr3 = (Cr + nn_line[off + 2] - hue) >> 1;
      YUV_TO_RGB<version>(Y, Cb3, Cr3, n_line, off);

      Y = n_line[off + 3];
      Cb = (Cb + Cb2 + Cb3 + nn_line[off + 1 + 6] - hue) >>
           2; // Left + Above + Right +Below
      Cr = (Cr + Cr2 + Cr3 + nn_line[off + 2 + 6] - hue) >> 2;
      YUV_TO_RGB<version>(Y, Cb, Cr, n_line, off + 3);
      off += 6;
    }
    int Y = c_line[off];
    int Cb = c_line[off + 1] - hue;
    int Cr = c_line[off + 2] - hue;
    YUV_TO_RGB<version>(Y, Cb, Cr, c_line, off);

    Y = c_line[off + 3];
    YUV_TO_RGB<version>(Y, Cb, Cr, c_line, off + 3);

    // Next line
    Y = n_line[off];
    Cb = (Cb + nn_line[off + 1] - hue) >> 1;
    Cr = (Cr + nn_line[off + 2] - hue) >> 1;
    YUV_TO_RGB<version>(Y, Cb, Cr, n_line, off);

    Y = n_line[off + 3];
    YUV_TO_RGB<version>(Y, Cb, Cr, n_line, off + 3);
  }

  if (atLastLine) {
    c_line = (ushort16*)mRaw->getData(0, end_h * 2);
    n_line = (ushort16*)mRaw->getData(0, end_h * 2 + 1);
    off = 0;

    // Last line
    for (int x = 0; x < w + 1; x++) {
      int Y = c_line[off];
      int Cb = c_line[off + 1] - hue;
      int Cr = c_line[off + 2] - hue;
      YUV_TO_RGB<version>(Y, Cb, Cr, c_line, off);

      Y = c_line[off + 3];
      YUV_TO_RGB<version>(Y, Cb, Cr, c_line, off + 3);

      // Next line
      Y = n_line[off];
      YUV_TO_RGB<version>(Y, Cb, Cr, n_line, off);

      Y = n_line[off + 3];
      YUV_TO_RGB<version>(Y, Cb, Cr, n_line, off + 3);
      off += 6;
    }
  }
}

inline void Cr2sRawInterpolator::STORE_RGB(ushort16* X, int r, int g, int b,
                                           int offset) {
  X[offset + 0] = clampBits(r >> 8, 16);
  X[offset + 1] = clampBits(g >> 8, 16);
  X[offset + 2] = clampBits(b >> 8, 16);
}

template </* int version */>
/* Algorithm found in EOS 40D */
inline void Cr2sRawInterpolator::YUV_TO_RGB<0>(int Y, int Cb, int Cr,
                                               ushort16* X, int offset) {
  int r, g, b;
  r = sraw_coeffs[0] * (Y + Cr - 512);
  g = sraw_coeffs[1] * (Y + ((-778 * Cb - (Cr * 2048)) >> 12) - 512);
  b = sraw_coeffs[2] * (Y + (Cb - 512));
  STORE_RGB(X, r, g, b, offset);
}

template </* int version */>
inline void Cr2sRawInterpolator::YUV_TO_RGB<1>(int Y, int Cb, int Cr,
                                               ushort16* X, int offset) {
  int r, g, b;
  r = sraw_coeffs[0] * (Y + ((50 * Cb + 22929 * Cr) >> 12));
  g = sraw_coeffs[1] * (Y + ((-5640 * Cb - 11751 * Cr) >> 12));
  b = sraw_coeffs[2] * (Y + ((29040 * Cb - 101 * Cr) >> 12));
  STORE_RGB(X, r, g, b, offset);
}

template </* int version */>
/* Algorithm found in EOS 5d Mk III */
inline void Cr2sRawInterpolator::YUV_TO_RGB<2>(int Y, int Cb, int Cr,
                                               ushort16* X, int offset) {
  int r, g, b;
  r = sraw_coeffs[0] * (Y + Cr);
  g = sraw_coeffs[1] * (Y + ((-778 * Cb - (Cr * 2048)) >> 12));
  b = sraw_coeffs[2] * (Y + Cb);
  STORE_RGB(X, r, g, b, offset);
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
  auto hue = -raw_hue + 16384;
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
    int width = mRaw->dim.x / subSampling.x;
    int height = mRaw->dim.y / subSampling.y;

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
