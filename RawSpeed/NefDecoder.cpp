#include "StdAfx.h"
#include "NefDecoder.h"

NefDecoder::NefDecoder(TiffIFD *rootIFD, FileMap* file ) : 
RawDecoder(file), mRootIFD(rootIFD)
{

}

NefDecoder::~NefDecoder(void)
{
}

RawImage NefDecoder::decodeRaw()
{
  vector<TiffIFD*> data = mRootIFD->getIFDsWithTag(CFAPATTERN);

  if (data.empty())
    ThrowRDE("NEF Decoder: No image data found");

  TiffIFD* raw = data[0];
  int compression = raw->getEntry(COMPRESSION)->getInt();
  if (34713 != compression)
    ThrowRDE("NEF Decoder: Unsupported compression");

  TiffEntry *offsets = raw->getEntry(STRIPOFFSETS);
  TiffEntry *counts = raw->getEntry(STRIPBYTECOUNTS);

  if (offsets->count != 1) {
    ThrowRDE("NEF Decoder: Multiple Strips found: %u",offsets->count);
  }
  if (counts->count != offsets->count) {
    ThrowRDE("NEF Decoder: Byte count number does not match strip size: count:%u, strips:%u ",counts->count, offsets->count);
  }
  guint width = raw->getEntry(IMAGEWIDTH)->getInt();
  guint height = raw->getEntry(IMAGELENGTH)->getInt();
  guint bitPerPixel = raw->getEntry(BITSPERSAMPLE)->getInt();
  return NULL;
}