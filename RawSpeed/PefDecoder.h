#pragma once
#include "RawDecoder.h"
#include "TiffIFD.h"
#include "PentaxDecompressor.h"

class PefDecoder :
  public RawDecoder
{
public:
  PefDecoder(TiffIFD *rootIFD, FileMap* file);
  virtual ~PefDecoder(void);
  RawImage decodeRaw();
  TiffIFD *mRootIFD;

};
