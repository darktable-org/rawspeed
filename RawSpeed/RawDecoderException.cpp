#include "StdAfx.h"
#include "RawDecoderException.h"



void ThrowRDE(const char* fmt, ...) {
  char buf[8192];
  va_list val;
  va_start(val, fmt);
  sprintf_s(buf, 8192, fmt, val);
  va_end(val);
  _RPT1(0, "EXCEPTION: %s\n",buf);
  throw RawDecoderException(buf);
}