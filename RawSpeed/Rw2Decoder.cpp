#include "StdAfx.h"
#include "Rw2Decoder.h"


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
  guint bitPerPixel = raw->getEntry(BITSPERSAMPLE)->getInt();
  return mRaw;
}

void Rw2Decoder::decodeMetaData( CameraMetaData *meta )
{

}