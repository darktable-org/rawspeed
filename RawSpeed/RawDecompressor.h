#pragma once
#include "RawDecompressorException.h"
#include "FileMap.h"
#include "BitPumpJPEG.h" // Includes bytestream
#include "RawImage.h"

class RawDecompressor 
{
public:
  RawDecompressor(FileMap* file);
  virtual ~RawDecompressor(void);
  virtual RawImage decodeRaw() = 0;
  FileMap *mFile; 
  void readUncompressedRaw(unsigned int offset, int max_offset, int* colorOrder);
  RawImage mRaw; 
  vector<const char*> errors;
protected:
};


