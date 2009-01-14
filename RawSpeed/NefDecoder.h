#pragma once
#include "RawDecoder.h"
#include "LJpegPlain.h"
#include "TiffIFD.h"
#include "BitPumpPlain.h"
#include "TiffParser.h"
#include "NikonDecompressor.h"

class NefDecoder :
  public RawDecoder
{
public:
  NefDecoder(TiffIFD *rootIFD, FileMap* file);
  virtual ~NefDecoder(void);
  virtual RawImage decodeRaw();
  TiffIFD *mRootIFD;
private:
  gboolean D100IsCompressed(guint offset);
  void DecodeUncompressed();
  void DecodeD100Uncompressed();
};

class NefSlice {
public:
  NefSlice() { h = offset = count = 0;};
  ~NefSlice() {};
  guint h;
  guint offset;
  guint count;
};