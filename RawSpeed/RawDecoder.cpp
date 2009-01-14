#include "StdAfx.h"
#include "RawDecoder.h"

RawDecoder::RawDecoder(FileMap* file) : mFile(file), mRaw(RawImage::create())
{
}

RawDecoder::~RawDecoder(void)
{

}

void RawDecoder::readUncompressedRaw(ByteStream &input, iPoint2D& size, iPoint2D& offset, int inputPitch, int bitPerPixel, gboolean MSBOrder) {
  guchar* data = mRaw->getData();
  guint outPitch = mRaw->pitch;
  const guchar *in = input.getData();
  guint w = size.x;
  guint h = size.y;
  if (input.getRemainSize()< (inputPitch*h) ) {
    h = input.getRemainSize() / inputPitch - 1;
  }
  if (bitPerPixel>16)
    ThrowRDE("readUncompressedRaw: Unsupported bit depth");

  guint skipBits = inputPitch - w*bitPerPixel/8;  // Skip per line
  if (offset.y>mRaw->dim.y)
    ThrowRDE("readUncompressedRaw: Invalid y offset");
  if (offset.x+size.x>mRaw->dim.x)
    ThrowRDE("readUncompressedRaw: Invalid x offset");

  guint y = offset.y;
  h = min(h+offset.y,mRaw->dim.y);

  if (MSBOrder) {
    BitPumpMSB bits(&input);
    for (; y < h; y++) {
      gushort* dest = (gushort*)&data[offset.x*sizeof(gushort)+y*outPitch];
      for(guint x =0 ; x < w; x++) {
        dest[x] = bits.getBits(bitPerPixel);
      }
      bits.skipBits(skipBits);
    }
  } else {
    BitPumpPlain bits(&input);
    for (; y < h; y++) {
      gushort* dest = (gushort*)&data[offset.x*sizeof(gushort)+y*outPitch];
      for(guint x =0 ; x < w; x++) {
        dest[x] = bits.getBits(bitPerPixel);
      }
      bits.skipBits(skipBits);
    }
  }
}

void RawDecoder::Decode12BitRaw(ByteStream &input, guint w, guint h) {
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