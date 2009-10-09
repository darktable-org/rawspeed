#include "StdAfx.h"
#include "OrfDecoder.h"
#include "TiffParserOlympus.h"
#ifdef __unix__
#include <stdlib.h>
#endif
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

OrfDecoder::OrfDecoder(TiffIFD *rootIFD, FileMap* file):
    RawDecoder(file), mRootIFD(rootIFD) {
}

OrfDecoder::~OrfDecoder(void) {
}

RawImage OrfDecoder::decodeRaw() {
  vector<TiffIFD*> data = mRootIFD->getIFDsWithTag(STRIPOFFSETS);

  if (data.empty())
    ThrowRDE("ORF Decoder: No image data found");

  TiffIFD* raw = data[0];

  int compression = raw->getEntry(COMPRESSION)->getInt();
  if (1 != compression)
    ThrowRDE("ORF Decoder: Unsupported compression");

  TiffEntry *offsets = raw->getEntry(STRIPOFFSETS);
  TiffEntry *counts = raw->getEntry(STRIPBYTECOUNTS);

  if (offsets->count != 1) {
    ThrowRDE("ORF Decoder: Multiple Strips found: %u", offsets->count);
  }
  if (counts->count != offsets->count) {
    ThrowRDE("ORF Decoder: Byte count number does not match strip size: count:%u, strips:%u ", counts->count, offsets->count);
  }
  guint width = raw->getEntry(IMAGEWIDTH)->getInt();
  guint height = raw->getEntry(IMAGELENGTH)->getInt();

  if (!mFile->isValid(offsets->getInt() + counts->getInt()))
    ThrowRDE("ORF Decoder: Truncated file");

  mRaw->dim = iPoint2D(width, height);
  mRaw->bpp = 2;
  mRaw->createData();

  data = mRootIFD->getIFDsWithTag(MAKERNOTE);
  if (data.empty())
    ThrowRDE("ORF Decoder: No Makernote found");

  TiffIFD* exif = data[0];
  TiffEntry *makernoteEntry = exif->getEntry(MAKERNOTE);
  const guchar* makernote = makernoteEntry->getData();
  FileMap makermap((guchar*)&makernote[8], makernoteEntry->count - 8);
  TiffParserOlympus makertiff(&makermap);
  makertiff.parseData();

  data = makertiff.RootIFD()->getIFDsWithTag((TiffTag)0x2010);

  if (data.empty())
    ThrowRDE("ORF Decoder: Unsupported compression");
  TiffEntry *oly = data[0]->getEntry((TiffTag)0x2010);
  if (oly->type == TIFF_UNDEFINED)
    ThrowRDE("ORF Decoder: Unsupported compression");

  // We add 3 bytes slack, since the bitpump might be a few bytes ahead.
  ByteStream s(mFile->getData(offsets->getInt()), counts->getInt() + 3);

  try {
    decodeCompressed(s, width, height);
  } catch (IOException) {
    // Let's ignore it, it may have delivered somewhat useful data.
  }

  return mRaw;
}
/* This is probably the slowest decoder of them all.
 * I cannot see any way to effectively speed up the prediction
 * phase, which is by far the slowest part of this algorithm.
 * Also there is no way to multithread this code, since prediction
 * is based on the output of all previous pixel (bar the first four)
 */

void OrfDecoder::decodeCompressed(ByteStream& s, guint w, guint h) {
  int nbits, sign, low, high, i, wo, n, nw;
  int acarry[2][3], *carry, pred, diff;

  guchar* data = mRaw->getData();
  gint pitch = mRaw->pitch;

  /* Build a table to quickly look up "high" value */
  gchar bittable[4096];
  for (i = 0; i < 4096; i++) {
    int b = i;
    for (high = 0; high < 12; high++)
      if ((b>>(11-high))&1)
        break;
      bittable[i] = high;
  }

  s.skipBytes(7);
  BitPumpMSB bits(&s);

  for (guint y = 0; y < h; y++) {
    memset(acarry, 0, sizeof acarry);
    gushort* dest = (gushort*) & data[y*pitch];
    for (guint x = 0; x < w; x++) {
      bits.checkPos();
      bits.fill();
      carry = acarry[x & 1];
      i = 2 * (carry[2] < 3);
      for (nbits = 2 + i; (gushort) carry[0] >> (nbits + i); nbits++);
      int b = bits.peekBitsNoFill(15);
      sign = (b >> 14) * -1;
      low  = (b >> 12) & 3;
      high = bittable[b&4095];
      // Skip bits used above.
      bits.skipBitsNoFill(min(12+3, high + 1 + 3));

      if (high == 12)
        high = bits.getBits(16 - nbits) >> 1;
      carry[0] = (high << nbits) | bits.getBits(nbits);
      diff = (carry[0] ^ sign) + carry[1];
      carry[1] = (diff * 3 + carry[1]) >> 5;
      carry[2] = carry[0] > 16 ? 0 : carry[2] + 1;
      if (y < 2 && x < 2) pred = 0;
      else if (y < 2) pred = dest[x-2];
      else if (x < 2) pred = dest[-pitch+((gint)x)];  // Pitch is in bytes, and dest is in short, so we skip two lines
      else {
        wo  = dest[x-2];
        n  = dest[-pitch+((gint)x)];
        nw = dest[-pitch+((gint)x)-2];
        if ((wo < nw && nw < n) || (n < nw && nw < wo)) {
          if (abs(wo - nw) > 32 || abs(n - nw) > 32)
            pred = wo + n - nw;
          else pred = (wo + n) >> 1;
        } else pred = abs(wo - nw) > abs(n - nw) ? wo : n;
      }
      dest[x] = pred + ((diff << 2) | low);
      _ASSERTE(0 == dest[x] >> 12) ;
    }
  }
}

void OrfDecoder::checkSupport(CameraMetaData *meta) {
  vector<TiffIFD*> data = mRootIFD->getIFDsWithTag(MODEL);
  if (data.empty())
    ThrowRDE("ORF Support check: Model name found");
  string make = data[0]->getEntry(MAKE)->getString();
  string model = data[0]->getEntry(MODEL)->getString();
  this->checkCameraSupported(meta, make, model, "");
}

void OrfDecoder::decodeMetaData(CameraMetaData *meta) {
  mRaw->cfa.setCFA(CFA_RED, CFA_GREEN, CFA_GREEN2, CFA_BLUE);
  vector<TiffIFD*> data = mRootIFD->getIFDsWithTag(MODEL);

  if (data.empty())
    ThrowRDE("ORF Meta Decoder: Model name found");

  string make = data[0]->getEntry(MAKE)->getString();
  string model = data[0]->getEntry(MODEL)->getString();

  setMetaData(meta, make, model, "");

}
