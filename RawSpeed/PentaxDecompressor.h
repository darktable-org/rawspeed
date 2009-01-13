#pragma once
#include "ljpegdecompressor.h"
#include "BitPumpMSB.h"

class PentaxDecompressor :
  public LJpegDecompressor
{
public:
  PentaxDecompressor(FileMap* file, RawImage img);
  virtual ~PentaxDecompressor(void);
  gint HuffDecodePentax();
  void decodePentax( guint offset, guint size );
  BitPumpMSB *pentaxBits;
};
