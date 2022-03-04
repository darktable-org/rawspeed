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

#include "io/FileReader.h"
#include "io/Buffer.h"          // for Buffer, Buffer::size_type
#include "io/FileIOException.h" // for ThrowFIE
#include <cstdio>               // for fseek, fclose, feof, ferror, fopen
#include <fcntl.h>              // for SEEK_END, SEEK_SET
#include <limits>               // for numeric_limits
#include <memory>               // for unique_ptr, make_unique, operator==
#include <utility>              // for move

#if !(defined(__unix__) || defined(__APPLE__))
#ifndef NOMINMAX
#define NOMINMAX // do not want the min()/max() macros!
#endif

#include "io/FileIO.h" // for widenFileName
#include <Windows.h>
#include <io.h>
#include <tchar.h>
#endif

namespace rawspeed {

std::unique_ptr<const Buffer> FileReader::readFile() {
  size_t fileSize = 0;

#if defined(__unix__) || defined(__APPLE__)
  using file_ptr = std::unique_ptr<FILE, decltype(&fclose)>;
  file_ptr file(fopen(fileName, "rb"), &fclose);

  if (file == nullptr)
    ThrowFIE("Could not open file \"%s\".", fileName);

  fseek(file.get(), 0, SEEK_END);
  const auto size = ftell(file.get());

  if (size <= 0)
    ThrowFIE("File is 0 bytes.");

  fileSize = size;

  if (fileSize > std::numeric_limits<Buffer::size_type>::max())
    ThrowFIE("File is too big (%zu bytes).", fileSize);

  fseek(file.get(), 0, SEEK_SET);

  auto dest = Buffer::Create(fileSize);

  if (auto bytes_read = fread(dest.get(), 1, fileSize, file.get());
      fileSize != bytes_read) {
    ThrowFIE("Could not read file, %s.",
             feof(file.get()) ? "reached end-of-file"
                              : (ferror(file.get()) ? "file reading error"
                                                    : "unknown problem"));
  }

#else // __unix__

  auto wFileName = widenFileName(fileName);

  using file_ptr = std::unique_ptr<std::remove_pointer<HANDLE>::type,
                                   decltype(&CloseHandle)>;
  file_ptr file(CreateFileW(wFileName.data(), GENERIC_READ, FILE_SHARE_READ,
                            nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN,
                            nullptr),
                &CloseHandle);

  if (file.get() == INVALID_HANDLE_VALUE)
    ThrowFIE("Could not open file \"%s\".", fileName);

  LARGE_INTEGER size;
  GetFileSizeEx(file.get(), &size);

  static_assert(
      std::numeric_limits<Buffer::size_type>::max() ==
          std::numeric_limits<decltype(size.LowPart)>::max(),
      "once Buffer migrates to 64-bit index, this needs to be updated.");

  if (size.HighPart > 0)
    ThrowFIE("File is too big.");
  if (size.LowPart <= 0)
    ThrowFIE("File is 0 bytes.");

  auto dest = Buffer::Create(size.LowPart);

  DWORD bytes_read;
  if (!ReadFile(file.get(), dest.get(), size.LowPart, &bytes_read, nullptr))
    ThrowFIE("Could not read file.");

  if (size.LowPart != bytes_read)
    ThrowFIE("Could not read file.");

  fileSize = size.LowPart;

#endif // __unix__

  return std::make_unique<Buffer>(std::move(dest), fileSize);
}

} // namespace rawspeed
