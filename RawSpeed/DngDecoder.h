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

class DngStrip {
public:
  DngStrip() { h = offset = count = offsetY = 0;};
  ~DngStrip() {};
  guint h;
  guint offset; // Offset in bytes
  guint count;
  guint offsetY;
};