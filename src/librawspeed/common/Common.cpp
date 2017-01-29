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

#if defined(__unix__) || defined(__APPLE__)
#ifdef _XOPEN_SOURCE
#if (_XOPEN_SOURCE < 600)
#undef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600 // for posix_memalign()
#endif                    // _XOPEN_SOURCE < 600
#else
#define _XOPEN_SOURCE 600 // for posix_memalign()
#endif                    //_XOPEN_SOURCE
#endif // defined(__unix__) || defined(__APPLE__)

#if defined(__APPLE__)
#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE
#endif // _DARWIN_C_SOURCE
#endif // defined(__APPLE__)

#include "common/Common.h"
#include <cstdarg> // for va_end, va_list, va_start
#include <cstdio>  // for vprintf
#include <cstdlib> // for posix_memalign, free

#if defined(__APPLE__)

#include <cstring>
#include <sys/sysctl.h>
#include <sys/types.h>

int macosx_version()
{
  static int ver = 0; // cached
  char str[256];
  size_t strsize = sizeof(str);
  if (0 == ver &&
      sysctlbyname("kern.osrelease", str, &strsize, nullptr, 0) == 0) {
    // kern.osrelease is a string formated as "Major.Minor.Patch"
    if (memchr(str, '\0', strsize) != nullptr) {
      int major, minor, patch;
      if (sscanf(str, "%d.%d.%d", &major, &minor, &patch) == 3) {
        ver = 0x1000 + major*10;
      }
    }
  }
  return ver;
}
void* _aligned_malloc(size_t bytes, size_t alignment) {

  if (macosx_version() >=0x1060) { // 10.6+
    void *ret = nullptr;
    if (0 == posix_memalign(&ret, alignment, bytes))
      return ret;
    else
      return nullptr;
  }
  return malloc(bytes); // Mac OS X malloc is usually aligned to 16 bytes
}

#elif defined(__unix__)

void* _aligned_malloc(size_t bytes, size_t alignment) {
  void *ret = nullptr;
  if (0 == posix_memalign(&ret, alignment, bytes))
    return ret;

  return nullptr;
}

#endif

#if defined(__APPLE__) || defined(__unix__)
void _aligned_free(void *ptr) { free(ptr); }
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

} // Namespace RawSpeed
