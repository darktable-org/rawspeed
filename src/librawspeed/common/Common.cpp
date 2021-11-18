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

#include "common/Common.h"
#include <cstdarg> // for va_end, va_list, va_start
#include <cstdio>  // for printf, vprintf

// #define _DEBUG

namespace rawspeed {

#if defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION) && !defined(_DEBUG)

void __attribute__((const))
writeLog(DEBUG_PRIO priority, const char* format, ...) {
  // When fuzzing, any output is really undesirable.
}

#else

void writeLog(DEBUG_PRIO priority, const char* format, ...) {
#ifndef _DEBUG
  if (priority < DEBUG_PRIO::INFO)
#endif // _DEBUG
    fprintf(stdout, "%s", "RawSpeed:");

  va_list args;
  va_start(args, format);

#ifndef _DEBUG
  if (priority < DEBUG_PRIO::INFO)
#endif // _DEBUG
    vfprintf(stdout, format, args);

  va_end(args);

#ifndef _DEBUG
  if (priority < DEBUG_PRIO::INFO)
#endif // _DEBUG
    fprintf(stdout, "%s", "\n");
}

#endif

} // namespace rawspeed
