#pragma once
#include "FileMap.h"
#include "TiffIFD.h"
#include "TiffIFDBE.h"
#include "TiffParserException.h"
//#include "ThumbnailGenerator.h"
#include "RawDecoder.h"
#include "DngDecoder.h"
#include "Cr2Decoder.h"
#include "ArwDecoder.h"
#include "PefDecoder.h"
#include "NefDecoder.h"
#include "OrfDecoder.h"
//#include "libgfl.h"

class TiffParser 
{
public:
  TiffParser(FileMap* input);
  virtual ~TiffParser(void);

  virtual void parseData();
  virtual RawDecoder* getDecompressor();
  Endianness endian;
  TiffIFD* RootIFD() const { return mRootIFD; }
protected:
  FileMap *mInput;
  TiffIFD* mRootIFD;
};

