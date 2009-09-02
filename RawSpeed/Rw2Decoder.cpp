#include "StdAfx.h"
#include "Rw2Decoder.h"

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

Rw2Decoder::Rw2Decoder(TiffIFD *rootIFD, FileMap* file) :
RawDecoder(file), mRootIFD(rootIFD), input(0),vbits(0)
{

}
Rw2Decoder::~Rw2Decoder(void)
{
  if (input)
    delete input;
  input = 0;
}

RawImage Rw2Decoder::decodeRaw()
{

  vector<TiffIFD*> data = mRootIFD->getIFDsWithTag(PANASONIC_STRIPOFFSET);

  if (data.empty())
    ThrowRDE("RW2 Decoder: No image data found");

  TiffIFD* raw = data[0];

  TiffEntry *offsets = raw->getEntry(PANASONIC_STRIPOFFSET);

  if (offsets->count != 1) {
    ThrowRDE("RW2 Decoder: Multiple Strips found: %u",offsets->count);
  }

  guint height = raw->getEntry((TiffTag)3)->getShort();
  guint width = raw->getEntry((TiffTag)2)->getShort();

  mRaw->dim = iPoint2D(width, height);
  mRaw->bpp = 2;
  mRaw->createData();

  load_flags = 0x2008;
  gint off = offsets->getInt();

  input = new ByteStream(mFile->getData(off),mFile->getSize()-off);
  try {
    DecodeRw2();
  } catch (IOException e) {
    // We attempt to ignore, since truncated files may be ok.
    errors.push_back(e.what());
  }

  return mRaw;
}

void Rw2Decoder::DecodeRw2()
{
  int x, y, i, j, sh=0, pred[2], nonz[2];
  int w = mRaw->dim.x;
  int h = mRaw->dim.y;

  for (y=0; y < h; y++) {
    gushort* dest = (gushort*)mRaw->getData(0,y);
    for (x=0; x < w; x++) {
      if ((i = x % 14) == 0)
        pred[0] = pred[1] = nonz[0] = nonz[1] = 0;
      if (i % 3 == 2) sh = 4 >> (3 - pana_bits(2));
      if (nonz[i & 1]) {
        if ((j = pana_bits(8))) {
          if ((pred[i & 1] -= 0x80 << sh) < 0 || sh == 4)
            pred[i & 1] &= ~(-1 << sh);
          pred[i & 1] += j << sh;
        }
      } else if ((nonz[i & 1] = pana_bits(8)) || i > 11)
        pred[i & 1] = nonz[i & 1] << 4 | pana_bits(4);
      dest[x] = pred[x&1];
    }
  }
}

guint Rw2Decoder::pana_bits (int nbits)
{
  int byte;

  if (!vbits) {
    if (input->getRemainSize() < 0x4000-load_flags) {
      memcpy (buf+load_flags, input->getData(), input->getRemainSize());
      input->skipBytes(input->getRemainSize());
    } else {
      memcpy (buf+load_flags, input->getData(), 0x4000-load_flags);
      input->skipBytes(0x4000-load_flags);
      if (input->getRemainSize()<load_flags) {
        memcpy (buf, input->getData(), input->getRemainSize());
        input->skipBytes(input->getRemainSize());
      } else {
        memcpy (buf, input->getData(), load_flags);
        input->skipBytes(load_flags);
      }
    }
  }
  vbits = (vbits - nbits) & 0x1ffff;
  byte = vbits >> 3 ^ 0x3ff0;
  return (buf[byte] | buf[byte+1] << 8) >> (vbits & 7) & ~(-1 << nbits);
}

void Rw2Decoder::checkSupport(CameraMetaData *meta) {
  vector<TiffIFD*> data = mRootIFD->getIFDsWithTag(MODEL);
  if (data.empty())
    ThrowRDE("RW2 Support check: Model name found");

  string make = data[0]->getEntry(MAKE)->getString();
  string model = data[0]->getEntry(MODEL)->getString();
  this->checkCameraSupported(meta, make, model, "");
}

void Rw2Decoder::decodeMetaData( CameraMetaData *meta )
{
  mRaw->cfa.setCFA(CFA_BLUE, CFA_GREEN, CFA_GREEN2, CFA_RED);
  vector<TiffIFD*> data = mRootIFD->getIFDsWithTag(MODEL);

  if (data.empty())
    ThrowRDE("CR2 Meta Decoder: Model name not found");

  string make = data[0]->getEntry(MAKE)->getString();
  string model = data[0]->getEntry(MODEL)->getString();
  string mode = getMode(model);

  printf("Mode: %s\n",mode.c_str());

  setMetaData(meta, make, model, mode);
}

bool Rw2Decoder::almostEqualRelative(float A, float B, float maxRelativeError)
{
  if (A == B)
    return true;

  float relativeError = fabs((A - B) / B);
  if (relativeError <= maxRelativeError)
    return true;
  return false;
}

std::string Rw2Decoder::getMode( const string model )
{
  float ratio = 3.0f / 2.0f;  // Default
  if (mRaw->isAllocated()) {
    ratio = (float)mRaw->dim.x / (float)mRaw->dim.y;
  }

  if (!model.compare("DMC-LX3") || !model.compare("DMC-G1") || !model.compare("DMC-GH1") || !model.compare("DMC-GF1")) {
    if (almostEqualRelative(ratio,16.0f/9.0f,0.02f))
      return "16:9";
    if (almostEqualRelative(ratio,3.0f/2.0f,0.02f))
      return "3:2";
    if (almostEqualRelative(ratio,4.0f/3.0f,0.02f))
      return "4:3";
    if (almostEqualRelative(ratio,1.0f,0.02f))
      return "1:1";
  }

  return "";
}
