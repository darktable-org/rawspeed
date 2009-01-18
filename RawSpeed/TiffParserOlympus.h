#pragma once
#include "TiffParser.h"

class TiffParserOlympus :
  public TiffParser
{
public:
  TiffParserOlympus(FileMap* input);
  virtual void parseData();
  virtual ~TiffParserOlympus(void);
};
