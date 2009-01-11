#include "StdAfx.h"
#include "ARWDecompressor.h"


ARWDecompressor::ARWDecompressor( TiffIFD *rootIFD, FileMap* file ) : 
 RawDecompressor(file), mRootIFD(rootIFD)
{

}

ARWDecompressor::~ARWDecompressor(void)
{
}

RawImage ARWDecompressor::decodeRaw()
{
  vector<TiffIFD*> data = mRootIFD->getIFDsWithTag(STRIPOFFSETS);

  if (data.empty())
    ThrowRDE("ARW Decoder: No image data found");

  TiffIFD* raw = data[0];
  TiffEntry *offsets = raw->getEntry(STRIPOFFSETS);
  TiffEntry *counts = raw->getEntry(STRIPBYTECOUNTS);

  if (offsets->count != 1) {
    ThrowRDE("ARW Decoder: Multiple Strips found: %u",offsets->count);
  }
  if (counts->count != offsets->count) {
    ThrowRDE("ARW Decoder: Byte count number does not match strip size: count:%u, strips:%u ",counts->count, offsets->count);
  }
  guint width = raw->getEntry(IMAGEWIDTH)->getInt();
  guint height = raw->getEntry(IMAGELENGTH)->getInt();
  guint bitPerPixel = raw->getEntry(BITSPERSAMPLE)->getInt();

  mRaw->dim = iPoint2D(width, height);
  mRaw->bpp = 2;
  mRaw->createData();

  const gushort* c = raw->getEntry(SONY_CURVE)->getShortArray();
  guint sony_curve[] = { 0,0,0,0,0,4095 };

  for (guint i = 0; i < 4; i++) 
    sony_curve[i+1] = (c[i]>> 2) & 0xfff;

  for (guint i = 0; i < 0x4001; i++) 
    curve[i] = i;

  for (guint i=0; i < 5; i++)
    for (guint j = sony_curve[i]+1; j <= sony_curve[i+1]; j++)
      curve[j] = curve[j-1] + (1 << i);

  ByteStream input(mFile->getData(offsets->getInt()), counts->getInt());

  if (raw->getEntry(COMPRESSION)->getInt() == 32767) {
    DecodeARW(input,width,height,bitPerPixel);
  } else {
    ThrowRDE("ARWDecompression: Unknown compression");
  }
  return mRaw;
}


void ARWDecompressor::DecodeARW(ByteStream &input, guint w, guint h, guint bpp) {
  guchar* data = mRaw->getData();
  guint pitch = mRaw->pitch;
  if (bpp == 8) {
    gushort pix[16];
    BitPumpPlain bits(&input);
    for (guint y = 0; y < h; y++ ) {
      gushort* dest = (gushort*)&data[y*pitch];
      bits.setAbsoluteOffset((w*bpp*y)>>3); // Realign
      for (guint x = 0; x < w-30; ) { // Process 32 pixels (16x2) per loop.
        bits.checkPos();
        gint _max = bits.getBits(11);
        gint _min = bits.getBits(11);
        gint _imax = bits.getBits(4);
        gint _imin = bits.getBits(4);
        guint sh;
        for (sh=0; sh < 4 && 0x80 << sh <= _max-_min; sh++);
        for (guint i=0; i < 16; i++) {
          if      (i == _imax) pix[i] = _max;
          else if (i == _imin) pix[i] = _min;
          else {
            pix[i] = (bits.getBits(7) << sh) + _min;
            if (pix[i] > 0x7ff) pix[i] = 0x7ff;
          }
        }
        for (guint i=0; i < 16; i++)
          dest[x+i*2] = curve[pix[i] << 1] >> 1;
        x += x & 1 ? 31 : 1;  // Skip to next 32 pixels
      }
    }
    return;
  } // End bpp = 8
  if (bpp==12) {
    const guchar *in = input.getData();
    if (input.getRemainSize()<(w*h*3/2)) {
      h = input.getRemainSize() / (w*3/2);
    }
    for (guint y=0; y < h; y++) {
      gushort* dest = (gushort*)&data[y*pitch];
      for(guint x =0 ; x < w; x+=2) {
        guint g1 = *in++;
        guint g2 = *in++;
        dest[x] = g1 | ((g2<<8)&0xf);
        guint g3 = *in++;
        dest[x+1] = (g2>>4) | (g3<<4);
      }
    }
    return;
  }
  ThrowRDE("Unsupported bit depth");
}