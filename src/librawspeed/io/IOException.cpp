/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include "io/IOException.h"
#include "common/Common.h" // for _RPT1
#include <cstdarg>         // for va_end, va_list, va_start
#include <cstdio>          // for vsnprintf
#include <string>          // for string

using namespace std;

namespace RawSpeed {

void ThrowIOE(const char* fmt, ...) {
  va_list val;
  va_start(val, fmt);
  static char buf[8192];
  vsnprintf(buf, 8192, fmt, val);
  va_end(val);
  writeLog(DEBUG_PRIO_EXTRA, "IO EXCEPTION: %s\n", buf);
  throw IOException(buf);
}


IOException::IOException( const char* _msg ) : std::runtime_error(string(_msg))
{
  writeLog(DEBUG_PRIO_EXTRA, "IO Exception: %s\n", _msg);
}

IOException::IOException(const string &_msg) : std::runtime_error(_msg) {
  writeLog(DEBUG_PRIO_EXTRA, "IO Exception: %s\n", _msg.c_str());
}
} // namespace RawSpeed
