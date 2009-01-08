#include "StdAfx.h"
#include "TiffParserException.h"

TiffParserException::TiffParserException(const string _msg) : runtime_error(_msg) {
  _RPT1(0, "TIFF Exception: %s\n", _msg.c_str());
};

