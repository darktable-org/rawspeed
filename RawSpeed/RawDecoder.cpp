#include "StdAfx.h"
#include "RawDecoder.h"

RawDecoder::RawDecoder(FileMap* file) : mFile(file), mRaw(RawImage::create())
{
}

RawDecoder::~RawDecoder(void)
{

}
void RawDecoder::readUncompressedRaw(unsigned int offset, int max_offset, int* colorOrder) {
  _ASSERTE(mRaw->dim.Area());
  if (mRaw->bpp != 2)
    ThrowRDE("RawDecoder: Unsupported bit depth.");
  // Prepare local variables.
  const unsigned short* raw = (const unsigned short*)mFile->getData(offset);
}

void RawDecoder::Decode12BitRaw(ByteStream &input, guint w, guint h, guint bpp) {
  guchar* data = mRaw->getData();
  guint pitch = mRaw->pitch;
  const guchar *in = input.getData();
  if (input.getRemainSize()< (w*h*3/2) ) {
    h = input.getRemainSize() / (w*3/2) - 1;
  }
  for (guint y=0; y < h; y++) {
    gushort* dest = (gushort*)&data[y*pitch];
    for(guint x =0 ; x < w; x+=2) {
      guint g1 = *in++;
      guint g2 = *in++;
      dest[x] = g1 | ((g2&0xf)<<8);
      guint g3 = *in++;
      dest[x+1] = (g2>>2) | (g3<<4);
    }
  }
}