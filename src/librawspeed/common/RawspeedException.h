/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2017 Roman Lebedev

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

#pragma once

#include "rawspeedconfig.h"

#include "common/Common.h"
#include <cstdarg>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace RawSpeed {

template <typename T>
[[noreturn]] static inline void __attribute__((noreturn, format(printf, 1, 2)))
ThrowException(const char* fmt, ...) {
  static constexpr size_t bufSize = 8192;
#if defined(HAVE_THREAD_LOCAL)
  static thread_local char buf[bufSize];
#elif defined(HAVE___THREAD)
  static __thread char buf[bufSize];
#else
#pragma message                                                                \
    "Don't have thread-local-storage! Exception text may be garbled if used multithreaded"
  static char buf[bufSize];
#endif

  va_list val;
  va_start(val, fmt);
  vsnprintf(buf, sizeof(buf), fmt, val);
  va_end(val);
  writeLog(DEBUG_PRIO_EXTRA, "EXCEPTION: %s", buf);
  throw T(buf);
}

class RawspeedException : public std::runtime_error {
private:
  void log(const char* msg) {
    writeLog(DEBUG_PRIO_EXTRA, "EXCEPTION: %s", msg);
  }

public:
  explicit RawspeedException(const std::string& msg) : std::runtime_error(msg) {
    log(msg.c_str());
  }
  explicit RawspeedException(const char* msg) : std::runtime_error(msg) {
    log(msg);
  }
};

#undef XSTR
#define XSTR(a) #a

#undef STR
#define STR(a) XSTR(a)

#ifndef DEBUG
#define ThrowExceptionHelper(CLASS, fmt, ...)                                  \
  ThrowException<CLASS>("%s, line " STR(__LINE__) ": " fmt,                    \
                        __PRETTY_FUNCTION__, ##__VA_ARGS__)
#else
#define ThrowExceptionHelper(CLASS, fmt, ...)                                  \
  ThrowException<CLASS>(__FILE__ ":" STR(__LINE__) ": %s: " fmt,               \
                        __PRETTY_FUNCTION__, ##__VA_ARGS__)
#endif

#define ThrowRSE(...) ThrowExceptionHelper(RawspeedException, __VA_ARGS__)

} // namespace RawSpeed
