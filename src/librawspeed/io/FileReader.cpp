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
#include "adt/AlignedAllocator.h"
#include "adt/Casts.h"
#include "adt/DefaultInitAllocatorAdaptor.h"
#include "io/Buffer.h"
#include "io/FileIOException.h"
#include <cstdint>
#include <cstdio>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#if !(defined(__unix__) || defined(__APPLE__))
#ifndef NOMINMAX
#define NOMINMAX // do not want the min()/max() macros!
#endif

#include "io/FileIO.h"
#include <Windows.h>
#include <io.h>
#include <tchar.h>
#endif

namespace rawspeed {

std::pair<std::unique_ptr<std::vector<
              uint8_t, DefaultInitAllocatorAdaptor<
                           uint8_t, AlignedAllocator<uint8_t, 16>>>>,
          Buffer>
FileReader::readFile() const {
  size_t fileSize = 0;

#if defined(__unix__) || defined(__APPLE__)
  auto fclose = [](std::FILE* fp) { std::fclose(fp); };
  using file_ptr = std::unique_ptr<FILE, decltype(fclose)>;
  file_ptr file(fopen(fileName, "rb"), fclose);

  if (file == nullptr)
    ThrowFIE("Could not open file \"%s\".", fileName);

  fseek(file.get(), 0, SEEK_END);
  const auto size = ftell(file.get());

  if (size <= 0)
    ThrowFIE("File is 0 bytes.");

  if (static_cast<int64_t>(size) >
      std::numeric_limits<Buffer::size_type>::max())
    ThrowFIE("File is too big (%zu bytes).", fileSize);

  fileSize = size;

  fseek(file.get(), 0, SEEK_SET);

  auto dest = std::make_unique<std::vector<
      uint8_t,
      DefaultInitAllocatorAdaptor<uint8_t, AlignedAllocator<uint8_t, 16>>>>(
      fileSize);

  if (auto bytes_read = fread(dest->data(), 1, fileSize, file.get());
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

  auto dest = std::make_unique<std::vector<
      uint8_t,
      DefaultInitAllocatorAdaptor<uint8_t, AlignedAllocator<uint8_t, 16>>>>(
      size.LowPart);

  DWORD bytes_read;
  if (!ReadFile(file.get(), dest->data(), size.LowPart, &bytes_read, nullptr))
    ThrowFIE("Could not read file.");

  if (size.LowPart != bytes_read)
    ThrowFIE("Could not read file.");

  fileSize = size.LowPart;

#endif // __unix__

  return {std::move(dest),
          Buffer(dest->data(), implicit_cast<Buffer::size_type>(fileSize))};
}

} // namespace rawspeed
