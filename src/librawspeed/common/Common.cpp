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

#ifndef NO_PTHREAD
#include <thread>  // for thread::hardware_concurrency
#endif

namespace RawSpeed {

void writeLog(int priority, const char *format, ...)
{
#ifndef _DEBUG
  if (priority < DEBUG_PRIO_INFO)
#endif // _DEBUG
    printf("%s", "RawSpeed:");

  va_list args;
  va_start(args, format);

#ifndef _DEBUG
  if(priority < DEBUG_PRIO_INFO)
#endif // _DEBUG
    vprintf(format, args);

  va_end(args);
}

// use a singleton type of method to allow the sucessfull read/write access
// of the static variable no matter when this is executed during static
// initialization of the translation unit / dll
static uint32 accessThreadCount(uint32 n = 0)
{
  static uint32 threadCount = 0;
  if (n)
    threadCount = n;
  return threadCount;
}

uint32 getThreadCount()
{
#ifdef NO_PTHREAD
  return 1;
#else
  auto n = accessThreadCount();
  return n ? n : std::max(1u, std::thread::hardware_concurrency());
#endif
}

void setThreadCount(uint32 n)
{
  accessThreadCount(n);
}

} // Namespace RawSpeed
