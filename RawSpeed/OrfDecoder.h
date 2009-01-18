#pragma once
#include "RawDecoder.h"
#include "LJpegPlain.h"
#include "TiffIFD.h"
#include "BitPumpPlain.h"


class OrfDecoder :
  public RawDecoder
{
public:
  OrfDecoder(TiffIFD *rootIFD, FileMap* file);
  virtual ~OrfDecoder(void);
  RawImage decodeRaw();
private:
  void decodeCompressed(ByteStream& s,guint w, guint h);
  TiffIFD *mRootIFD;

};
