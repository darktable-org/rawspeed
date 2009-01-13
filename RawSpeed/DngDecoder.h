#pragma once
#include "LJpegPlain.h"
#include "TiffIFD.h"


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
