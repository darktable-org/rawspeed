#pragma once
#include "RawDecompressor.h"
#include "LJpegPlain.h"
#include "TiffIFD.h"
#include "BitPumpPlain.h"

class ARWDecompressor :
  public RawDecompressor
{
public:
  ARWDecompressor(TiffIFD *rootIFD, FileMap* file);
  virtual ~ARWDecompressor(void);
  virtual RawImage decodeRaw();
protected:
  void DecodeARW(ByteStream &input, guint w, guint h);
  void DecodeARW2(ByteStream &input, guint w, guint h, guint bpp);
  TiffIFD *mRootIFD;
  guint curve[0x4001];
};
