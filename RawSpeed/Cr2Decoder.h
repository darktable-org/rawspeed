#pragma once
#include "RawDecoder.h"
#include "LJpegPlain.h"
#include "TiffIFD.h"

class Cr2Decoder :
  public RawDecoder
{
public:
  Cr2Decoder(TiffIFD *rootIFD, FileMap* file);
  virtual RawImage decodeRaw();
  virtual ~Cr2Decoder(void);
protected:
  TiffIFD *mRootIFD;

};

class Cr2Slice {
public:
  Cr2Slice() { w = h = offset = count = 0;};
  ~Cr2Slice() {};
  guint w;
  guint h;
  guint offset;
  guint count;
};