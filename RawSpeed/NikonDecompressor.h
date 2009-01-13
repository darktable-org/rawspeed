#pragma once
#include "ljpegdecompressor.h"

class NikonDecompressor :
  public LJpegDecompressor
{
public:
  NikonDecompressor(FileMap* file, RawImage img );
public:
  virtual ~NikonDecompressor(void);
};
