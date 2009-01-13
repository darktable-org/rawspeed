#pragma once
#include "FileMap.h"
#include "TiffIFD.h"
#include "TiffIFDBE.h"
#include "TiffParserException.h"
#include "ThumbnailGenerator.h"
#include "RawDecoder.h"
#include "DngDecoder.h"
#include "Cr2Decoder.h"
#include "ARWDecoder.h"
#include "PefDecoder.h"
#include "NefDecoder.h"

#include "libgfl.h"

class TiffParser 
{
public:
  TiffParser(FileMap* input);
  virtual ~TiffParser(void);

  void parseData();
  RawDecoder* getDecompressor();
  Endianness endian;
private:
  FileMap *mInput;
  TiffIFD* mRootIFD;
};

