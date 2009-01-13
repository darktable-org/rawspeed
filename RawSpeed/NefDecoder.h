#pragma once
#include "RawDecoder.h"
#include "LJpegPlain.h"
#include "TiffIFD.h"
#include "BitPumpPlain.h"

class NefDecoder :
  public RawDecoder
{
public:
  NefDecoder(TiffIFD *rootIFD, FileMap* file);
  virtual ~NefDecoder(void);
  virtual RawImage decodeRaw();
  TiffIFD *mRootIFD;
  guint curve[0x4001];
};
