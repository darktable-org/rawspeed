#pragma once
#include "RawDecompressor.h"
#include "LJpegPlain.h"
#include "TiffIFD.h"

class Cr2Decompressor :
  public RawDecompressor
{
public:
  Cr2Decompressor(TiffIFD *rootIFD, FileMap* file);
  virtual RawImage decodeRaw();
  virtual ~Cr2Decompressor(void);
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