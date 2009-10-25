#include "StdAfx.h"
#include "ArwDecoder.h"
/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009 Klaus Post

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

    http://www.klauspost.com
*/

namespace RawSpeed {

ArwDecoder::ArwDecoder(TiffIFD *rootIFD, FileMap* file) :
    RawDecoder(file), mRootIFD(rootIFD) {

}

ArwDecoder::~ArwDecoder(void) {
}

RawImage ArwDecoder::decodeRaw() {
  vector<TiffIFD*> data = mRootIFD->getIFDsWithTag(STRIPOFFSETS);

  data = mRootIFD->getIFDsWithTag(STRIPOFFSETS);

  if (data.empty())
    ThrowRDE("ARW Decoder: No image data found");

  TiffIFD* raw = data[0];
  int compression = raw->getEntry(COMPRESSION)->getInt();
  if (32767 != compression)
    ThrowRDE("ARW Decoder: Unsupported compression");

  TiffEntry *offsets = raw->getEntry(STRIPOFFSETS);
  TiffEntry *counts = raw->getEntry(STRIPBYTECOUNTS);

  if (offsets->count != 1) {
    ThrowRDE("ARW Decoder: Multiple Strips found: %u", offsets->count);
  }
  if (counts->count != offsets->count) {
    ThrowRDE("ARW Decoder: Byte count number does not match strip size: count:%u, strips:%u ", counts->count, offsets->count);
  }
  guint width = raw->getEntry(IMAGEWIDTH)->getInt();
  guint height = raw->getEntry(IMAGELENGTH)->getInt();
  guint bitPerPixel = raw->getEntry(BITSPERSAMPLE)->getInt();

  gboolean arw1 = counts->getInt() * 8 != width * height * bitPerPixel;
  if (arw1)
    height += 8;

  mRaw->dim = iPoint2D(width, height);
  mRaw->bpp = 2;
  mRaw->createData();

  const gushort* c = raw->getEntry(SONY_CURVE)->getShortArray();
  guint sony_curve[] = { 0, 0, 0, 0, 0, 4095 };

  for (guint i = 0; i < 4; i++)
    sony_curve[i+1] = (c[i] >> 2) & 0xfff;

  for (guint i = 0; i < 0x4001; i++)
    curve[i] = i;

  for (guint i = 0; i < 5; i++)
    for (guint j = sony_curve[i] + 1; j <= sony_curve[i+1]; j++)
      curve[j] = curve[j-1] + (1 << i);

  guint c2 = counts->getInt();
  guint off = offsets->getInt();
  if (!mFile->isValid(off + c2))
    c2 = mFile->getSize() - off;

  ByteStream input(mFile->getData(off), c2);

  if (arw1)
    DecodeARW(input, width, height);
  else
    DecodeARW2(input, width, height, bitPerPixel);

  return mRaw;
}

void ArwDecoder::DecodeARW(ByteStream &input, guint w, guint h) {
  BitPumpMSB bits(&input);
  guchar* data = mRaw->getData();
  gushort* dest = (gushort*) & data[0];
  guint pitch = mRaw->pitch / sizeof(gushort);
  gint sum = 0;
  for (guint x = w; x--;)
    for (guint y = 0; y < h + 1; y += 2) {
      bits.checkPos();
      bits.fill();
      if (y == h) y = 1;
      guint len = 4 - bits.getBitsNoFill(2);
      if (len == 3 && bits.getBitNoFill()) len = 0;
      if (len == 4)
        while (len < 17 && !bits.getBitNoFill()) len++;
      bits.fill();
      gint diff = bits.getBitsNoFill(len);
      if ((diff & (1 << (len - 1))) == 0)
        diff -= (1 << len) - 1;
      sum += diff;
      _ASSERTE(!(sum >> 12));
      if (y < h) dest[x+y*pitch] = sum;
    }
}

void ArwDecoder::DecodeARW2(ByteStream &input, guint w, guint h, guint bpp) {

  if (bpp == 8) {
    in = &input;
    this->startThreads();
    return;
  } // End bpp = 8

  if (bpp == 12) {
    guchar* data = mRaw->getData();
    guint pitch = mRaw->pitch;
    const guchar *in = input.getData();

    if (input.getRemainSize() < (w*h*3 / 2))
      h = input.getRemainSize() / (w * 3 / 2) - 1;

    for (guint y = 0; y < h; y++) {
      gushort* dest = (gushort*) & data[y*pitch];
      for (guint x = 0 ; x < w; x += 2) {
        guint g1 = *in++;
        guint g2 = *in++;
        // Shift up to match compressed precision
        dest[x] = (g1 | ((g2 & 0xf) << 8)) << 2;
        guint g3 = *in++;
        dest[x+1] = ((g2 >> 4) | (g3 << 4)) << 2;
      }
    }
    return;
  }
  ThrowRDE("Unsupported bit depth");
}

void ArwDecoder::checkSupport(CameraMetaData *meta) {
  vector<TiffIFD*> data = mRootIFD->getIFDsWithTag(MODEL);
  if (data.empty())
    ThrowRDE("ARW Support check: Model name found");
  string make = data[0]->getEntry(MAKE)->getString();
  string model = data[0]->getEntry(MODEL)->getString();
  this->checkCameraSupported(meta, make, model, "");
}

void ArwDecoder::decodeMetaData(CameraMetaData *meta) {
  //Default
  mRaw->cfa.setCFA(CFA_RED, CFA_GREEN, CFA_GREEN2, CFA_BLUE);
  vector<TiffIFD*> data = mRootIFD->getIFDsWithTag(MODEL);

  if (data.empty())
    ThrowRDE("ARW Meta Decoder: Model name found");

  string make = data[0]->getEntry(MAKE)->getString();
  string model = data[0]->getEntry(MODEL)->getString();

  setMetaData(meta, make, model, "");
}

/* Since ARW2 compressed images have predictable offsets, we decode them threaded */

void ArwDecoder::decodeThreaded(RawDecoderThread * t) {
  guchar* data = mRaw->getData();
  guint pitch = mRaw->pitch;
  guint w = mRaw->dim.x;

  BitPumpPlain bits(in);
  for (guint y = t->start_y; y < t->end_y; y++) {
    gushort* dest = (gushort*) & data[y*pitch];
    // Realign
    bits.setAbsoluteOffset((w*8*y) >> 3);

    // Process 32 pixels (16x2) per loop.
    for (guint x = 0; x < w - 30;) {
      bits.checkPos();
      gint _max = bits.getBits(11);
      gint _min = bits.getBits(11);
      gint _imax = bits.getBits(4);
      gint _imin = bits.getBits(4);
      gint sh;
      for (sh = 0; sh < 4 && 0x80 << sh <= _max - _min; sh++);
      for (gint i = 0; i < 16; i++) {
        gint p;
        if (i == _imax) p = _max;
        else if (i == _imin) p = _min;
        else {
          p = (bits.getBits(7) << sh) + _min;
          if (p > 0x7ff)
            p = 0x7ff;
        }
        dest[x+i*2] = curve[p << 1];
      }
      x += x & 1 ? 31 : 1;  // Skip to next 32 pixels
    }
  }
}

} // namespace RawSpeed
