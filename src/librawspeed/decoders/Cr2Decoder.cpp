/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2015-2017 Roman Lebedev
    Copyright (C) 2017 Axel Waggershauser

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

#include "decoders/Cr2Decoder.h"
#include "common/Common.h"
#include "common/Point.h"
#include "tiff/TiffEntry.h"
#include "tiff/TiffTag.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cfloat>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

namespace RawSpeed {

RawImage Cr2Decoder::decodeOldFormat() {
  uint32 offset = 0;
  if (mRootIFD->getEntryRecursive(CANON_RAW_DATA_OFFSET))
    offset = mRootIFD->getEntryRecursive(CANON_RAW_DATA_OFFSET)->getInt();
  else {
    // D2000 is oh so special...
    vector<TiffIFD*> data = mRootIFD->getIFDsWithTag(CFAPATTERN);
    if (data.empty() || ! data[0]->hasEntry(STRIPOFFSETS))
      ThrowRDE("CR2 Decoder: Couldn't find offset");

    offset = data[0]->getEntry(STRIPOFFSETS)->getInt();
  }

  ByteStream b(mFile, offset+41, getHostEndianness() == big);
  int height = b.getShort();
  int width = b.getShort();

  // some old models (1D/1DS/D2000C) encode two lines as one
  // see: FIX_CANON_HALF_HEIGHT_DOUBLE_WIDTH
  if (width > 2*height) {
    height *= 2;
    width /= 2;
  }
  width *= 2; // components

  mRaw = RawImage::create({width, height});

  LJpegPlain l(*mFile, offset, mRaw);
  l.addSlices({width});

  try {
    l.decode(0, 0);
  } catch (IOException& e) {
    mRaw->setError(e.what());
  }

  // deal with D2000 GrayResponseCurve
  TiffEntry* curve = mRootIFD->getEntryRecursive((TiffTag)0x123);
  if (curve && curve->type == TIFF_SHORT && curve->count == 4096) {
    auto* table = new ushort16[curve->count];
    curve->getShortArray(table, curve->count);
    if (!uncorrectedRawValues) {
      mRaw->setTable(table, curve->count, true);
      // Apply table
      mRaw->sixteenBitLookup();
      // Delete table
      mRaw->setTable(nullptr);
    } else {
      // We want uncorrected, but we store the table.
      mRaw->setTable(table, curve->count, false);
    }
    delete [] table;
  }

  return mRaw;
}

// for technical details about Cr2 mRAW/sRAW, see http://lclevy.free.fr/cr2/

RawImage Cr2Decoder::decodeNewFormat() {
  TiffEntry* sensorInfoE = mRootIFD->getEntryRecursive(CANON_SENSOR_INFO);
  if (!sensorInfoE)
    ThrowTPE("Cr2Decoder: failed to get SensorInfo from MakerNote");
  iPoint2D dim(sensorInfoE->getShort(1), sensorInfoE->getShort(2));

  int componentsPerPixel = 1;
  TiffIFD* raw = mRootIFD->getSubIFDs()[3].get();
  if (raw->hasEntry(CANON_SRAWTYPE) &&
      raw->getEntry(CANON_SRAWTYPE)->getInt() == 4)
    componentsPerPixel = 3;

  mRaw = RawImage::create(dim, TYPE_USHORT16, componentsPerPixel);

  vector<int> s_width;
  TiffEntry* cr2SliceEntry = raw->getEntryRecursive(CANONCR2SLICE);
  if (cr2SliceEntry && cr2SliceEntry->getShort(0) > 0) {
    for (int i = 0; i < cr2SliceEntry->getShort(0); i++)
      s_width.push_back(cr2SliceEntry->getShort(1));
    s_width.push_back(cr2SliceEntry->getShort(2));
  }

  TiffEntry* offsets = raw->getEntry(STRIPOFFSETS);
  TiffEntry* counts = raw->getEntry(STRIPBYTECOUNTS);

  LJpegPlain l(*mFile, offsets->getInt(), counts->getInt(), mRaw);
  l.addSlices(s_width);

  try {
    l.decode(0, 0);
  } catch (RawDecoderException &e) {
    mRaw->setError(e.what());
  } catch (IOException &e) {
    // Let's try to ignore this - it might be truncated data, so something might be useful.
    mRaw->setError(e.what());
  }

  if (mRaw->metadata.subsampling.x > 1 || mRaw->metadata.subsampling.y > 1)
    sRawInterpolate();

  return mRaw;
}

RawImage Cr2Decoder::decodeRawInternal() {
  if (mRootIFD->getSubIFDs().size() < 4)
    return decodeOldFormat();
  else // NOLINT ok, here it make sense
    return decodeNewFormat();
}

void Cr2Decoder::checkSupportInternal(CameraMetaData *meta) {
  vector<TiffIFD*> data = mRootIFD->getIFDsWithTag(MODEL);
  if (data.empty())
    ThrowRDE("CR2 Support check: Model name not found");
  if (!data[0]->hasEntry(MAKE))
    ThrowRDE("CR2 Support: Make name not found");
  string make = data[0]->getEntry(MAKE)->getString();
  string model = data[0]->getEntry(MODEL)->getString();

  // Check for sRaw mode
  if (mRootIFD->getSubIFDs().size() == 4) {
    TiffEntry* typeE = mRootIFD->getSubIFDs()[3]->getEntryRecursive(CANON_SRAWTYPE);
    if (typeE && typeE->getInt() == 4) {
      this->checkCameraSupported(meta, make, model, "sRaw1");
      return;
    }
  }

  this->checkCameraSupported(meta, make, model, "");
}

void Cr2Decoder::decodeMetaDataInternal(CameraMetaData *meta) {
  int iso = 0;
  mRaw->cfa.setCFA(iPoint2D(2,2), CFA_RED, CFA_GREEN, CFA_GREEN, CFA_BLUE);
  vector<TiffIFD*> data = mRootIFD->getIFDsWithTag(MODEL);

  if (data.empty())
    ThrowRDE("CR2 Meta Decoder: Model name not found");

  string make = data[0]->getEntry(MAKE)->getString();
  string model = data[0]->getEntry(MODEL)->getString();
  string mode;

  if (mRaw->metadata.subsampling.y == 2 && mRaw->metadata.subsampling.x == 2)
    mode = "sRaw1";

  if (mRaw->metadata.subsampling.y == 1 && mRaw->metadata.subsampling.x == 2)
    mode = "sRaw2";

  if (mRootIFD->hasEntryRecursive(ISOSPEEDRATINGS))
    iso = mRootIFD->getEntryRecursive(ISOSPEEDRATINGS)->getInt();

  // Fetch the white balance
  try{
    if (mRootIFD->hasEntryRecursive(CANONCOLORDATA)) {
      TiffEntry *wb = mRootIFD->getEntryRecursive(CANONCOLORDATA);
      // this entry is a big table, and different cameras store used WB in
      // different parts, so find the offset, starting with the most common one
      int offset = 126;

      // replace it with a hint if it exists
      if (hints.find("wb_offset") != hints.end()) {
        stringstream wb_offset(hints.find("wb_offset")->second);
        wb_offset >> offset;
      }

      offset /= 2;
      mRaw->metadata.wbCoeffs[0] = (float) wb->getShort(offset + 0);
      mRaw->metadata.wbCoeffs[1] = (float) wb->getShort(offset + 1);
      mRaw->metadata.wbCoeffs[2] = (float) wb->getShort(offset + 3);
    } else {
      if (mRootIFD->hasEntryRecursive(CANONSHOTINFO) &&
          mRootIFD->hasEntryRecursive(CANONPOWERSHOTG9WB)) {
        TiffEntry *shot_info = mRootIFD->getEntryRecursive(CANONSHOTINFO);
        TiffEntry *g9_wb = mRootIFD->getEntryRecursive(CANONPOWERSHOTG9WB);

        ushort16 wb_index = shot_info->getShort(7);
        int wb_offset = (wb_index < 18) ? "012347800000005896"[wb_index]-'0' : 0;
        wb_offset = wb_offset*8 + 2;

        mRaw->metadata.wbCoeffs[0] = (float) g9_wb->getInt(wb_offset+1);
        mRaw->metadata.wbCoeffs[1] = ((float) g9_wb->getInt(wb_offset+0) + (float) g9_wb->getInt(wb_offset+3)) / 2.0f;
        mRaw->metadata.wbCoeffs[2] = (float) g9_wb->getInt(wb_offset+2);
      } else if (mRootIFD->hasEntryRecursive((TiffTag) 0xa4)) {
        // WB for the old 1D and 1DS
        TiffEntry *wb = mRootIFD->getEntryRecursive((TiffTag) 0xa4);
        if (wb->count >= 3) {
          mRaw->metadata.wbCoeffs[0] = wb->getFloat(0);
          mRaw->metadata.wbCoeffs[1] = wb->getFloat(1);
          mRaw->metadata.wbCoeffs[2] = wb->getFloat(2);
        }
      }
    }
  } catch (const std::exception& e) {
    mRaw->setError(e.what());
    // We caught an exception reading WB, just ignore it
  }
  setMetaData(meta, make, model, mode, iso);
}

int Cr2Decoder::getHue() {
  if (hints.find("old_sraw_hue") != hints.end())
    return (mRaw->metadata.subsampling.y * mRaw->metadata.subsampling.x);

  if (!mRootIFD->hasEntryRecursive((TiffTag)0x10)) {
    return 0;
  }
  uint32 model_id = mRootIFD->getEntryRecursive((TiffTag)0x10)->getInt();
  if (model_id >= 0x80000281 || model_id == 0x80000218 || (hints.find("force_new_sraw_hue") != hints.end()))
    return ((mRaw->metadata.subsampling.y * mRaw->metadata.subsampling.x) - 1) >> 1;

  return (mRaw->metadata.subsampling.y * mRaw->metadata.subsampling.x);
}

// Interpolate and convert sRaw data.
void Cr2Decoder::sRawInterpolate() {
  vector<TiffIFD*> data = mRootIFD->getIFDsWithTag(CANONCOLORDATA);
  if (data.empty())
    ThrowRDE("CR2 sRaw: Unable to locate WB info.");

  TiffEntry *wb = data[0]->getEntry(CANONCOLORDATA);
  // Offset to sRaw coefficients used to reconstruct uncorrected RGB data.
  uint32 offset = 78;

  sraw_coeffs[0] = wb->getShort(offset+0);
  sraw_coeffs[1] = (wb->getShort(offset+1) + wb->getShort(offset+2) + 1) >> 1;
  sraw_coeffs[2] = wb->getShort(offset+3);

  if (hints.find("invert_sraw_wb") != hints.end()) {
    sraw_coeffs[0] = (int)(1024.0f/((float)sraw_coeffs[0]/1024.0f));
    sraw_coeffs[2] = (int)(1024.0f/((float)sraw_coeffs[2]/1024.0f));
  }

  /* Determine sRaw coefficients */
  bool isOldSraw = hints.find("sraw_40d") != hints.end();
  bool isNewSraw = hints.find("sraw_new") != hints.end();

  if (mRaw->metadata.subsampling.y == 1 && mRaw->metadata.subsampling.x == 2) {
    if (isOldSraw)
      interpolate_422_v0(mRaw->dim.x / 2, mRaw->dim.y, 0, mRaw->dim.y);
    else if (isNewSraw)
      interpolate_422_v2(mRaw->dim.x / 2, mRaw->dim.y, 0, mRaw->dim.y);
    else
      interpolate_422_v1(mRaw->dim.x / 2, mRaw->dim.y, 0, mRaw->dim.y);
  } else if (mRaw->metadata.subsampling.y == 2 && mRaw->metadata.subsampling.x == 2) {
    if (isNewSraw)
      interpolate_420_v2(mRaw->dim.x / 2, mRaw->dim.y / 2, 0, mRaw->dim.y / 2);
    else
      interpolate_420_v1(mRaw->dim.x / 2, mRaw->dim.y / 2, 0, mRaw->dim.y / 2);
  } else
    ThrowRDE("CR2 Decoder: Unknown subsampling");
}

/* sRaw interpolators - ugly as sin, but does the job in reasonably speed */

// Note: Thread safe.

template <typename T>
static inline void interpolate_422(T yuv2rgb, const int* sraw_coeffs,
                                   RawImage& mRaw, int hue, int hue_last, int w,
                                   int h, int start_h, int end_h) {
  // Last pixel should not be interpolated
  w--;

  // Current line
  ushort16* c_line;

  for (int y = start_h; y < end_h; y++) {
    c_line = (ushort16*)mRaw->getData(0, y);
    int off = 0;
    for (int x = 0; x < w; x++) {
      int Y = c_line[off];
      int Cb = c_line[off+1] - hue;
      int Cr = c_line[off+2] - hue;
      yuv2rgb(Y, Cb, Cr, sraw_coeffs, c_line, off);
      off += 3;

      Y = c_line[off];
      int Cb2 = (Cb + c_line[off+1+3] - hue) >> 1;
      int Cr2 = (Cr + c_line[off+2+3] - hue) >> 1;
      yuv2rgb(Y, Cb2, Cr2, sraw_coeffs, c_line, off);
      off += 3;
    }
    // Last two pixels
    int Y = c_line[off];
    int Cb = c_line[off + 1] - hue_last;
    int Cr = c_line[off + 2] - hue_last;
    yuv2rgb(Y, Cb, Cr, sraw_coeffs, c_line, off);

    Y = c_line[off+3];
    yuv2rgb(Y, Cb, Cr, sraw_coeffs, c_line, off + 3);
  }
}

// Note: Not thread safe, since it writes inplace.
template <typename T>
static inline void interpolate_420(T yuv2rgb, const int* sraw_coeffs,
                                   RawImage& mRaw, int hue, int w, int h,
                                   int start_h, int end_h) {
  // Last pixel should not be interpolated
  w--;

  bool atLastLine = false;

  if (end_h == h) {
    end_h--;
    atLastLine = true;
  }

  // Current line
  ushort16* c_line;
  // Next line
  ushort16* n_line;
  // Next line again
  ushort16* nn_line;

  int off;

  for (int y = start_h; y < end_h; y++) {
    c_line = (ushort16*)mRaw->getData(0, y * 2);
    n_line = (ushort16*)mRaw->getData(0, y * 2 + 1);
    nn_line = (ushort16*)mRaw->getData(0, y * 2 + 2);
    off = 0;
    for (int x = 0; x < w; x++) {
      int Y = c_line[off];
      int Cb = c_line[off+1] - hue;
      int Cr = c_line[off+2] - hue;
      yuv2rgb(Y, Cb, Cr, sraw_coeffs, c_line, off);

      Y = c_line[off+3];
      int Cb2 = (Cb + c_line[off+1+6] - hue) >> 1;
      int Cr2 = (Cr + c_line[off+2+6] - hue) >> 1;
      yuv2rgb(Y, Cb2, Cr2, sraw_coeffs, c_line, off + 3);

      // Next line
      Y = n_line[off];
      int Cb3 = (Cb + nn_line[off+1] - hue) >> 1;
      int Cr3 = (Cr + nn_line[off+2] - hue) >> 1;
      yuv2rgb(Y, Cb3, Cr3, sraw_coeffs, n_line, off);

      Y = n_line[off+3];
      Cb = (Cb + Cb2 + Cb3 + nn_line[off+1+6] - hue) >> 2;  //Left + Above + Right +Below
      Cr = (Cr + Cr2 + Cr3 + nn_line[off+2+6] - hue) >> 2;
      yuv2rgb(Y, Cb, Cr, sraw_coeffs, n_line, off + 3);
      off += 6;
    }
    int Y = c_line[off];
    int Cb = c_line[off+1] - hue;
    int Cr = c_line[off+2] - hue;
    yuv2rgb(Y, Cb, Cr, sraw_coeffs, c_line, off);

    Y = c_line[off+3];
    yuv2rgb(Y, Cb, Cr, sraw_coeffs, c_line, off + 3);

    // Next line
    Y = n_line[off];
    Cb = (Cb + nn_line[off+1] - hue) >> 1;
    Cr = (Cr + nn_line[off+2] - hue) >> 1;
    yuv2rgb(Y, Cb, Cr, sraw_coeffs, n_line, off);

    Y = n_line[off+3];
    yuv2rgb(Y, Cb, Cr, sraw_coeffs, n_line, off + 3);
  }

  if (atLastLine) {
    c_line = (ushort16*)mRaw->getData(0, end_h * 2);
    n_line = (ushort16*)mRaw->getData(0, end_h * 2 + 1);
    off = 0;

    // Last line
    for (int x = 0; x < w; x++) {
      int Y = c_line[off];
      int Cb = c_line[off+1] - hue;
      int Cr = c_line[off+2] - hue;
      yuv2rgb(Y, Cb, Cr, sraw_coeffs, c_line, off);

      Y = c_line[off+3];
      yuv2rgb(Y, Cb, Cr, sraw_coeffs, c_line, off + 3);

      // Next line
      Y = n_line[off];
      yuv2rgb(Y, Cb, Cr, sraw_coeffs, n_line, off);

      Y = n_line[off+3];
      yuv2rgb(Y, Cb, Cr, sraw_coeffs, n_line, off + 3);
      off += 6;
    }
  }
}

static inline void STORE_RGB(ushort16* X, int r, int g, int b, int offset) {
  r >>= 8;
  g >>= 8;
  b >>= 8;
  X[offset + 0] = clampbits(r, 16);
  X[offset + 1] = clampbits(g, 16);
  X[offset + 2] = clampbits(b, 16);
}

static inline void YUV_TO_RGB_v1(int Y, int Cb, int Cr, const int* sraw_coeffs,
                                 ushort16* X, int offset) {
  int r, g, b;
  r = sraw_coeffs[0] * (Y + ((50 * Cb + 22929 * Cr) >> 12));
  g = sraw_coeffs[1] * (Y + ((-5640 * Cb - 11751 * Cr) >> 12));
  b = sraw_coeffs[2] * (Y + ((29040 * Cb - 101 * Cr) >> 12));
  STORE_RGB(X, r, g, b, offset);
}

void Cr2Decoder::interpolate_422_v1(int w, int h, int start_h, int end_h) {
  auto hue = -getHue() + 16384;
  interpolate_422(YUV_TO_RGB_v1, sraw_coeffs, mRaw, hue, hue, w, h, start_h,
                  end_h);
}

void Cr2Decoder::interpolate_420_v1(int w, int h, int start_h, int end_h) {
  auto hue = -getHue() + 16384;
  interpolate_420(YUV_TO_RGB_v1, sraw_coeffs, mRaw, hue, w, h, start_h, end_h);
}

/* Algorithm found in EOS 40D */
static inline void YUV_TO_RGB_v0(int Y, int Cb, int Cr, const int* sraw_coeffs,
                                 ushort16* X, int offset) {
  int r, g, b;
  r = sraw_coeffs[0] * (Y + Cr - 512);
  g = sraw_coeffs[1] * (Y + ((-778 * Cb - (Cr << 11)) >> 12) - 512);
  b = sraw_coeffs[2] * (Y + (Cb - 512));
  STORE_RGB(X, r, g, b, offset);
}

void Cr2Decoder::interpolate_422_v0(int w, int h, int start_h, int end_h) {
  auto hue = -getHue() + 16384;
  auto hue_last = 16384;
  interpolate_422(YUV_TO_RGB_v0, sraw_coeffs, mRaw, hue, hue_last, w, h,
                  start_h, end_h);
}

/* Algorithm found in EOS 5d Mk III */
static inline void YUV_TO_RGB_v2(int Y, int Cb, int Cr, const int* sraw_coeffs,
                                 ushort16* X, int offset) {
  int r, g, b;
  r = sraw_coeffs[0] * (Y + Cr);
  g = sraw_coeffs[1] * (Y + ((-778 * Cb - (Cr << 11)) >> 12));
  b = sraw_coeffs[2] * (Y + Cb);
  STORE_RGB(X, r, g, b, offset);
}

void Cr2Decoder::interpolate_422_v2(int w, int h, int start_h, int end_h) {
  auto hue = -getHue() + 16384;
  interpolate_422(YUV_TO_RGB_v2, sraw_coeffs, mRaw, hue, hue, w, h, start_h,
                  end_h);
}

void Cr2Decoder::interpolate_420_v2(int w, int h, int start_h, int end_h) {
  auto hue = -getHue() + 16384;
  interpolate_420(YUV_TO_RGB_v2, sraw_coeffs, mRaw, hue, w, h, start_h, end_h);
}

} // namespace RawSpeed
