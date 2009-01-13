#pragma once
#include "RawDecoderException.h"
#include "FileMap.h"
#include "BitPumpJPEG.h" // Includes bytestream
#include "RawImage.h"

class RawDecoder 
{
public:
  RawDecoder(FileMap* file);
  virtual ~RawDecoder(void);
  virtual RawImage decodeRaw() = 0;
  FileMap *mFile; 
  void readUncompressedRaw(unsigned int offset, int max_offset, int* colorOrder);
  RawImage mRaw; 
  vector<const char*> errors;
protected:
};


