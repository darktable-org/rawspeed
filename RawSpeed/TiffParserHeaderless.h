#pragma once
#include "TiffParser.h"

class TiffParserHeaderless :
  public TiffParser
{
public:
  TiffParserHeaderless(FileMap* input, Endianness _end);
  virtual ~TiffParserHeaderless(void);
  void parseData(guint firstIfdOffset);
  virtual void parseData();
};
