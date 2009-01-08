#pragma once
#include "ljpegdecompressor.h"
#include "TiffIFD.h"


class DngDecompressor : 
  public RawDecompressor
{
public:
  DngDecompressor(TiffIFD *rootIFD, FileMap* file);
  virtual ~DngDecompressor(void);
  virtual RawImage decodeRaw();
protected:
  TiffIFD *mRootIFD;
};
