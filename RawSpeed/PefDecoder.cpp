#include "StdAfx.h"
#include "PefDecoder.h"

PefDecoder::PefDecoder(TiffIFD *rootIFD, FileMap* file) :
RawDecoder(file), mRootIFD(rootIFD)
{
}

PefDecoder::~PefDecoder(void)
{
}

RawImage PefDecoder::decodeRaw()
{
  vector<TiffIFD*> data = mRootIFD->getIFDsWithTag(STRIPOFFSETS);

  if (data.empty())
    ThrowRDE("PEF Decoder: No image data found");

  TiffIFD* raw = data[0];

  int compression = raw->getEntry(COMPRESSION)->getInt();
  if (65535 != compression)
    ThrowRDE("PEF Decoder: Unsupported compression");

  TiffEntry *offsets = raw->getEntry(STRIPOFFSETS);
  TiffEntry *counts = raw->getEntry(STRIPBYTECOUNTS);

  if (offsets->count != 1) {
    ThrowRDE("PEF Decoder: Multiple Strips found: %u",offsets->count);
  }
  if (counts->count != offsets->count) {
    ThrowRDE("PEF Decoder: Byte count number does not match strip size: count:%u, strips:%u ",counts->count, offsets->count);
  }
  if (!mFile->isValid(offsets->getInt()+counts->getInt()))
    ThrowRDE("PEF Decoder: Truncated file.");

  guint width = raw->getEntry(IMAGEWIDTH)->getInt();
  guint height = raw->getEntry(IMAGELENGTH)->getInt();
  guint bitPerPixel = raw->getEntry(BITSPERSAMPLE)->getInt();

  mRaw->dim = iPoint2D(width, height);
  mRaw->bpp = 2;
  mRaw->createData();
  
  PentaxDecompressor l(mFile,mRaw);
  l.decodePentax(offsets->getInt(), counts->getInt());

  return mRaw;
}