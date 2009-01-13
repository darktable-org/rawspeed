#pragma once
#include "ljpegdecompressor.h"
#include "BitPumpMSB.h"

/******************
 * Decompresses Lossless non subsampled JPEGs, with 2-4 components
 *****************/

class LJpegPlain :
  public LJpegDecompressor
{
public:
  LJpegPlain(FileMap* file, RawImage img);
  virtual ~LJpegPlain(void);
protected:
  virtual void decodeScan();
private:
  void decodeScanLeft4Comps();
  void decodeScanLeft2Comps();
  void decodeScanLeft3Comps();
};
