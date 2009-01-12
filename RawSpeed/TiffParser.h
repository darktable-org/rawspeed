#pragma once
#include "FileMap.h"
#include "TiffIFD.h"
#include "TiffIFDBE.h"
#include "TiffParserException.h"
#include "ThumbnailGenerator.h"
#include "RawDecompressor.h"
#include "DngDecompressor.h"
#include "Cr2Decompressor.h"
#include "ARWDecompressor.h"
#include "PefDecompressor.h"
#include "libgfl.h"

class TiffParser 
{
public:
  TiffParser(FileMap* input);
  virtual ~TiffParser(void);

  void parseData();
  RawDecompressor* getDecompressor();
  //RgbImage* readPreview(PreviewType preferedPreview);
  Endianness endian;
private:
  FileMap *mInput;
  TiffIFD* mRootIFD;
  GFL_BITMAP *thumb_bmp;
};

