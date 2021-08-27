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

#include "io/FileWriter.h"
#include "io/Buffer.h"          // for Buffer
#include "io/FileIOException.h" // for ThrowFIE
#include <cstdio>               // for fclose, fopen, fwrite, FILE, size_t

#if !defined(__unix__) && !defined(__APPLE__)
#ifndef NOMINMAX
#define NOMINMAX // do not want the min()/max() macros!
#endif

#include "io/FileIO.h" // for widenFileName
#include <Windows.h>
#include <io.h>
#include <tchar.h>
#endif // !defined(__unix__) && !defined(__APPLE__)

namespace rawspeed {

FileWriter::FileWriter(const char *_filename) : mFilename(_filename) {}

void FileWriter::writeFile(Buffer& filemap, uint32_t size) {
  if (size > filemap.getSize())
    size = filemap.getSize();
#if defined(__unix__) || defined(__APPLE__)
  size_t bytes_written = 0;
  FILE *file;

  file = fopen(mFilename, "wb");
  if (file == nullptr)
    ThrowFIE("Could not open file.");

  const auto* const src = filemap.getData(0, filemap.getSize());
  bytes_written = fwrite(src, 1, size != 0 ? size : filemap.getSize(), file);
  fclose(file);
  if (size != bytes_written) {
    ThrowFIE("Could not write file.");
  }

#else // __unix__
  auto wFileName = widenFileName(mFilename);
  HANDLE file_h;  // File handle
  file_h =
      CreateFileW(wFileName.data(), GENERIC_WRITE, FILE_SHARE_WRITE, nullptr,
                  CREATE_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
  if (file_h == INVALID_HANDLE_VALUE) {
    ThrowFIE("Could not open file.");
  }

  DWORD bytes_written;
  if (!WriteFile(file_h, filemap.getData(0, filemap.getSize()),
                 size ? size : filemap.getSize(), &bytes_written, nullptr)) {
    CloseHandle(file_h);
    ThrowFIE("Could not read file.");
  }
  CloseHandle(file_h);

#endif // __unix__
}

} // namespace rawspeed
