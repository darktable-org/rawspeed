#include "StdAfx.h"
#include "RawDecoderException.h"



void ThrowRDE(const char* fmt, ...) {
  va_list val;
  va_start(val, fmt);
  char buf[8192];
  vsprintf_s(buf, 8192, fmt, val);
  va_end(val);
  _RPT1(0, "EXCEPTION: %s\n",buf);
  throw RawDecoderException(buf);
}