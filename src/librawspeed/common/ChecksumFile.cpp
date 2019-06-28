/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2018 Roman Lebedev

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

#include "common/ChecksumFile.h"
#include "common/Common.h"            // for splitString
#include "common/RawspeedException.h" // for ThrowRSE
#include "io/Buffer.h"                // for Buffer
#include "io/FileReader.h"            // for FileReader
#include <cassert>                    // for assert
#include <memory>                     // for unique_ptr
#include <string>                     // for string, allocator, operator+
#include <vector>                     // for vector

namespace rawspeed {

namespace {

// The length of the sha1 digest (160-bit, 40 hexadecimal chars).
constexpr auto Sha1CheckSumLength = 40;
// The separator after the digest and before filename.
// Should be either "  " or " b".
constexpr auto CheckSumSeparatorWidth = 2;

ChecksumFileEntry ParseChecksumFileLine(const std::string& Line,
                                        const std::string& RootDir) {
  ChecksumFileEntry Entry;

  // We are just assuming that the checksum file is correct and valid.
  // It is up to user to validate it first (via actually running `sha1sum -c`).

  static constexpr auto Offset = Sha1CheckSumLength + CheckSumSeparatorWidth;

  if (Line.size() <= Offset)
    ThrowRSE("Malformed checksum line: \"%s\"", Line.c_str());

  Entry.RelFileName = Line.substr(Offset);
  assert(!Entry.RelFileName.empty());
  assert(Entry.RelFileName.back() != '\n');

  Entry.FullFileName = RootDir + "/" + Entry.RelFileName;

  return Entry;
}

} // namespace

std::vector<ChecksumFileEntry>
ParseChecksumFileContent(const std::string& ChecksumFileContent,
                         const std::string& RootDir) {
  std::vector<ChecksumFileEntry> Listing;

  const std::vector<std::string> Lines = splitString(ChecksumFileContent, '\n');

  Listing.reserve(Lines.size());

  for (const auto& Line : Lines) {
    assert(!Line.empty());
    Listing.emplace_back(ParseChecksumFileLine(Line, RootDir));
  }

  return Listing;
}

std::vector<ChecksumFileEntry>
ReadChecksumFile(const std::string& RootDir,
                 const std::string& ChecksumFileBasename) {
  const std::string ChecksumFileName = RootDir + "/" + ChecksumFileBasename;
  FileReader FR(ChecksumFileName.c_str());

  std::unique_ptr<const Buffer> buf = FR.readFile();
  const std::string ChecksumFileContent(
      reinterpret_cast<const char*>(buf->begin()), buf->getSize());

  return ParseChecksumFileContent(ChecksumFileContent, RootDir);
}

} // namespace rawspeed
