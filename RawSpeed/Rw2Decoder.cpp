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
RawDecoder(file), mRootIFD(rootIFD)
{

}
Rw2Decoder::~Rw2Decoder(void)
{
}

RawImage Rw2Decoder::decodeRaw()
{
  ThrowRDE("RW2 Decoder: Not supported");
/*
  vector<TiffIFD*> data = mRootIFD->getIFDsWithTag(PANASONIC_STRIPOFFSET);

  if (data.empty())
    ThrowRDE("RW2 Decoder: No image data found");

  TiffIFD* raw = data[0];

  TiffEntry *offsets = raw->getEntry(PANASONIC_STRIPOFFSET);

  if (offsets->count != 1) {
    ThrowRDE("RW2 Decoder: Multiple Strips found: %u",offsets->count);
  }

//  guint width = raw->getEntry()->getInt();
  guint height = raw->getEntry((TiffTag)3)->getInt();
  guint bitPerPixel = raw->getEntry(BITSPERSAMPLE)->getInt();*/
  return NULL;
}

void Rw2Decoder::checkSupport(CameraMetaData *meta) {
  ThrowRDE("RW2 Decoder: Not supported");
/*  vector<TiffIFD*> data = mRootIFD->getIFDsWithTag(MODEL);
  if (data.empty())
    ThrowRDE("PEF Support check: Model name found");

  string make = data[0]->getEntry(MAKE)->getString();
  string model = data[0]->getEntry(MODEL)->getString();
  this->checkCameraSupported(meta, make, model, "");*/
}

void Rw2Decoder::decodeMetaData( CameraMetaData *meta )
{

}