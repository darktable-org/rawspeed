#include "StdAfx.h"
#include "Cr2Decoder.h"
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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA

    http://www.klauspost.com
*/

Cr2Decoder::Cr2Decoder(TiffIFD *rootIFD, FileMap* file) :
 RawDecoder(file), mRootIFD(rootIFD) 
{

}

Cr2Decoder::~Cr2Decoder(void)
{
}

RawImage Cr2Decoder::decodeRaw()
{
  vector<TiffIFD*> data = mRootIFD->getIFDsWithTag((TiffTag)0xc5d8);

  if (data.empty())
    ThrowRDE("CR2 Decoder: No image data found");

  TiffIFD* raw = data[0];
  mRaw = RawImage::create();
  mRaw->isCFA = true;
  vector<Cr2Slice> slices;
  int completeH = 0;

  try {
    TiffEntry *offsets = raw->getEntry(STRIPOFFSETS);
    TiffEntry *counts = raw->getEntry(STRIPBYTECOUNTS);
    // Iterate through all slices
    for (guint s = 0; s<offsets->count; s++) {
      Cr2Slice slice;
      slice.offset = offsets[0].getInt();
      slice.count = counts[0].getInt();
      SOFInfo sof;
      LJpegPlain l(mFile,mRaw);
      l.getSOF(&sof,slice.offset,slice.count);
      slice.w = sof.w*sof.cps;
      slice.h = sof.h;
      if (!slices.empty())
        if (slices[0].w != slice.w)
          ThrowRDE("CR2 Decoder: Slice width does not match.");

      if (mFile->isValid(slice.offset+slice.count)) // Only decode if size is valid
        slices.push_back(slice);
      completeH += slice.h;
    }
  } catch (TiffParserException) {
    ThrowRDE("CR2 Decoder: Unsupported format.");
  }
  
  if (slices.empty()) {
    ThrowRDE("CR2 Decoder: No Slices found.");
  }

  mRaw->bpp = 2;
  mRaw->dim = iPoint2D(slices[0].w, completeH);
  mRaw->createData();


  vector<int> s_width;
  if (raw->hasEntry(CANONCR2SLICE)) {
    const gushort *ss = raw->getEntry(CANONCR2SLICE)->getShortArray();
    for (int i = 0; i < ss[0]; i++) {
      s_width.push_back(ss[1]);
    }
    s_width.push_back(ss[2]);
  } else {
    s_width.push_back(slices[0].w);
  }
  guint offY = 0;

  for (guint i = 0; i < slices.size(); i++ ) {  // This loop is obvious for threading, as slices are independent
    Cr2Slice slice = slices[i];
    try {
      LJpegPlain l(mFile,mRaw);
      l.addSlices(s_width);
      l.mUseBigtable = true;
      l.startDecoder(slice.offset, slice.count, 0, offY);
    } catch (RawDecoderException* e) { 
      // These may just be single slice error - store the error and move on
      errors.push_back(_strdup(e->what()));
    }
    offY += slice.w;
  }

  return mRaw;
}

void Cr2Decoder::decodeMetaData() {
  mRaw->cfa.setCFA(CFA_GREEN, CFA_BLUE, CFA_RED, CFA_GREEN);
  vector<TiffIFD*> data = mRootIFD->getIFDsWithTag(MODEL);

  if (data.empty())
    ThrowRDE("CR2 Decoder: Model name found");

  if (!data[0]->getEntry(MODEL)->getString().compare("Canon PowerShot G10")) {
    mRaw->cfa.setCFA(CFA_RED,CFA_GREEN,CFA_GREEN,CFA_BLUE);
  }

}