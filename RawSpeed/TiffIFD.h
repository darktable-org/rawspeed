#pragma once
#include "FileMap.h"
#include "TiffEntry.h"
#include "TiffParserException.h"

typedef enum Endianness {
  big, little
} Endianness;


class TiffIFD
{
public:
  TiffIFD();
  TiffIFD(FileMap* f, guint offset);
  virtual ~TiffIFD(void);
  vector<TiffIFD*> mSubIFD;
  map<TiffTag, TiffEntry*> mEntry;
  gint getNextIFD() {return nextIFD;}
  vector<TiffIFD*> getIFDsWithTag(TiffTag tag);
  TiffEntry* getEntry(TiffTag tag);
  bool hasEntry(TiffTag tag);
protected:
  gint nextIFD;
};




