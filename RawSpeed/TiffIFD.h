#pragma once
#include "FileMap.h"
#include "tiffEntry.h"
#include "TiffParserException.h"

typedef enum Endianness {
  big, little
};


class TiffIFD
{
public:
  TiffIFD();
  TiffIFD(FileMap* f, guint offset);
  ~TiffIFD(void);
  vector<TiffIFD> mSubIFD;
  map<TiffTag, TiffEntry*> mEntry;
  gint getNextIFD() {return nextIFD;}
  vector<TiffIFD*> getIFDsWithTag(TiffTag tag);
  TiffEntry* getEntry(TiffTag tag);
  gboolean hasEntry(TiffTag tag);
protected:
  gint nextIFD;
};




