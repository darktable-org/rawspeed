#pragma once
#include "RawDecoderException.h"
#include "FileMap.h"
#include "BitPumpJPEG.h" // Includes bytestream
#include "RawImage.h"
#include "BitPumpMSB.h"
#include "BitPumpPlain.h"
#include "CameraMetaData.h"

class RawDecoder 
{
public:
  RawDecoder(FileMap* file);
  virtual ~RawDecoder(void);
  virtual RawImage decodeRaw() = 0;
  virtual void decodeMetaData(CameraMetaData *meta) = 0;
  FileMap *mFile; 
  void readUncompressedRaw(ByteStream &input, iPoint2D& size, iPoint2D& offset, int inputPitch, int bitPerPixel, gboolean MSBOrder);
  RawImage mRaw; 
  vector<const char*> errors;
protected:
  virtual void setMetaData(CameraMetaData *meta, string make, string model);
  void Decode12BitRaw(ByteStream &input, guint w, guint h);
  void TrimSpaces( string& str);
};


