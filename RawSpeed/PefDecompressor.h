#pragma once
#include "rawdecompressor.h"
#include "TiffIFD.h"
#include "LJpegPlain.h"

class PefDecompressor :
  public RawDecompressor
{
public:
  PefDecompressor(TiffIFD *rootIFD, FileMap* file);
  virtual ~PefDecompressor(void);
  RawImage decodeRaw();
  TiffIFD *mRootIFD;

};
