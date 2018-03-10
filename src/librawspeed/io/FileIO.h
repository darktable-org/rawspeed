/*
    RawSpeed - RAW file decoder.

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

#if !(defined(__unix__) || defined(__APPLE__))
#define NOMINMAX // do not want the min()/max() macros!

#include "io/FileIOException.h" // for FileIOException (ptr only), ThrowFIE
#include <Windows.h>
#include <functional> // for bind
#include <io.h>
#include <tchar.h>
#include <vector> // for vector

namespace rawspeed {

inline std::wstring widenFileName(const char* fileName) {
  assert(fileName);

  std::wstring wFileName;

  auto f = std::bind(MultiByteToWideChar, CP_UTF8, 0, fileName, -1,
                     std::placeholders::_1, std::placeholders::_2);

  // how many wide characters are needed to store converted string?
  const auto expectedLen = f(nullptr, 0);
  wFileName.resize(expectedLen);

  // convert.
  const auto actualLen = f(&wFileName[0], wFileName.size());

  // did we get expected number of characters?
  if (actualLen != expectedLen)
    ThrowFIE("Could not convert filename \"%s\".", fileName);

  return wFileName;
}

} // namespace rawspeed

#endif
