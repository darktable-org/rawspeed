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

#include "io/FileReader.h"
#include "io/Buffer.h"          // for Buffer
#include "io/FileIOException.h" // for FileIOException
#include <algorithm>            // for move
#include <cstdio>               // for fclose, fseek, fopen, fread, ftell
#include <fcntl.h>              // for SEEK_END, SEEK_SET
#include <memory>               // for unique_ptr

#if !defined(__unix__) && !defined(__APPLE__)
#include <io.h>
#include <tchar.h>
#include <windows.h>
#endif // !defined(__unix__) && !defined(__APPLE__)

namespace rawspeed {

FileReader::FileReader(const char *_filename) : mFilename(_filename) {}

Buffer* FileReader::readFile() {
#if defined(__unix__) || defined(__APPLE__)
  int bytes_read = 0;
  FILE *file;
  long size;

  file = fopen(mFilename, "rb");
  if (file == nullptr)
    ThrowFIE("Could not open file.");
  fseek(file, 0, SEEK_END);
  size = ftell(file);
  if (size <= 0) {
    fclose(file);
    ThrowFIE("File is 0 bytes.");
  }
  fseek(file, 0, SEEK_SET);

  auto dest = Buffer::Create(size);
  bytes_read = fread(dest.get(), 1, size, file);
  fclose(file);
  if (size != bytes_read)
    ThrowFIE("Could not read file.");

  auto* fileData = new Buffer(move(dest), size);

#else // __unix__

  HANDLE file_h;  // File handle
  file_h = CreateFile(mFilename, GENERIC_READ, FILE_SHARE_READ, nullptr,
                      OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
  if (file_h == INVALID_HANDLE_VALUE) {
    ThrowFIE("Could not open file.");
  }

  LARGE_INTEGER f_size;
  GetFileSizeEx(file_h , &f_size);

  if (!f_size.LowPart)
    ThrowFIE("File is 0 bytes.");

  auto dest = Buffer::Create(f_size.LowPart);

  DWORD bytes_read;
  if (!ReadFile(file_h, dest.get(), f_size.LowPart, &bytes_read, nullptr)) {
    CloseHandle(file_h);
    ThrowFIE("Could not read file.");
  }

  CloseHandle(file_h);

  auto* fileData = new Buffer(move(dest), f_size.LowPart);

#endif // __unix__

  return fileData;
}

} // namespace rawspeed
