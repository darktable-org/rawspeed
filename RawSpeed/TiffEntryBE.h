#pragma once
#include "TiffEntry.h"

class TiffEntryBE :
  public TiffEntry
{
public:
//  TiffEntryBE(void);
  TiffEntryBE(FileMap* f, guint offset);
  virtual ~TiffEntryBE(void);
  virtual guint getInt();
  virtual gushort getShort();
  virtual const guint* getIntArray();
  virtual const gushort* getShortArray();
private:
  bool mDataSwapped;
};

