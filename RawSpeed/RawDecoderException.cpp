#include "StdAfx.h"
#include "RawDecoderException.h"
#ifndef __WIN32__
#include <stdarg.h>
#define vsprintf_s(...) vsnprintf(__VA_ARGS__)
#endif



void ThrowRDE(const char* fmt, ...) {
  va_list val;
  va_start(val, fmt);
  char buf[8192];
  vsprintf_s(buf, 8192, fmt, val);
  va_end(val);
  _RPT1(0, "EXCEPTION: %s\n",buf);
  throw RawDecoderException(buf);
}
