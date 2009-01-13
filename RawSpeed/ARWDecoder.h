#pragma once
#include "RawDecoder.h"
#include "LJpegPlain.h"
#include "TiffIFD.h"
#include "BitPumpPlain.h"

class ARWDecoder :
  public RawDecoder
{
public:
  ARWDecoder(TiffIFD *rootIFD, FileMap* file);
  virtual ~ARWDecoder(void);
  virtual RawImage decodeRaw();
protected:
  void DecodeARW(ByteStream &input, guint w, guint h);
  void DecodeARW2(ByteStream &input, guint w, guint h, guint bpp);
  TiffIFD *mRootIFD;
  guint curve[0x4001];
};
