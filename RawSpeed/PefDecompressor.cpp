#include "StdAfx.h"
#include "PefDecompressor.h"

PefDecompressor::PefDecompressor(TiffIFD *rootIFD, FileMap* file) :
RawDecompressor(file), mRootIFD(rootIFD)
{
}

PefDecompressor::~PefDecompressor(void)
{
}

RawImage PefDecompressor::decodeRaw()
{
  vector<TiffIFD*> data = mRootIFD->getIFDsWithTag(STRIPOFFSETS);

  if (data.empty())
    ThrowRDE("PEF Decoder: No image data found");

  TiffIFD* raw = data[0];
  TiffEntry *offsets = raw->getEntry(STRIPOFFSETS);
  TiffEntry *counts = raw->getEntry(STRIPBYTECOUNTS);

  if (offsets->count != 1) {
    ThrowRDE("PEF Decoder: Multiple Strips found: %u",offsets->count);
  }
  if (counts->count != offsets->count) {
    ThrowRDE("PEF Decoder: Byte count number does not match strip size: count:%u, strips:%u ",counts->count, offsets->count);
  }
  guint width = raw->getEntry(IMAGEWIDTH)->getInt();
  guint height = raw->getEntry(IMAGELENGTH)->getInt();
  guint bitPerPixel = raw->getEntry(BITSPERSAMPLE)->getInt();

  mRaw->dim = iPoint2D(width, height);
  mRaw->bpp = 2;
  mRaw->createData();
  
  LJpegPlain l(mFile,mRaw);
  l.decodePentax(offsets->getInt(), counts->getInt());

  return mRaw;
}