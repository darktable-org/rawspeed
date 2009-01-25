#pragma once
#include "LJpegPlain.h"
#include "TiffIFD.h"
#include "DngDecoderSlices.h"

class DngDecoder : 
  public RawDecoder
{
public:
  DngDecoder(TiffIFD *rootIFD, FileMap* file);
  virtual ~DngDecoder(void);
  virtual RawImage decodeRaw();
protected:
  TiffIFD *mRootIFD;
  gboolean mFixLjpeg;
};
