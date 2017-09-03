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

#include "io/FileIOException.h" // for FileIOException (ptr only), ThrowFIE
#include <functional>           // for bind
#include <io.h>
#include <string> // for string, wstring
#include <tchar.h>
#include <windows.h>

namespace rawspeed {

template <typename Conv, typename Tin, typename Tout>
inline Tout convertFileName(Tin fileName, Conv&& converter, UINT CodePage) {
  Tout cFileName;

  auto f = std::bind(converter, CodePage, 0, &fileName[0], -1,
                     std::placeholders::_1, std::placeholders::_2);

  // how many characters are needed to store converted string?
  const auto expectedLen = f(nullptr, 0);
  cFileName.resize(expectedLen);

  // convert.
  const auto actualLen = f(&cFileName[0], cFileName.size());

  // did we get expected number of characters?
  if (actualLen != expectedLen)
    ThrowFIE("Could not convert filename \"%s\".", fileName.c_str());

  return cFileName;
}

inline std::wstring widenFileName(std::string fileName,
                                  UINT CodePage = CP_UTF8) {
  return convertFileName(fileName, MultiByteToWideChar, CodePage);
}

inline std::string unwidenFileName(std::wstring fileName,
                                   UINT CodePage = CP_UTF8) {
  return convertFileName(fileName, WideCharToMultiByte, CodePage);
}

inline std::string ANSIfileNameToUTF8(std::string fileName) {
  return unwidenFileName(widenFileName(fileName, CP_ACP), CP_UTF8);
}

} // namespace rawspeed

#endif
