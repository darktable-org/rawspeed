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

#pragma once

#include <cstdint> // for uint32_t

namespace rawspeed {

class Buffer;

class FileWriter
{
public:
  explicit FileWriter(const char* filename);

  void writeFile(Buffer* fileMap, uint32_t size = 0);
  const char* Filename() const { return mFilename; }
  //  void Filename(const char * val) { mFilename = val; }

private:
  const char* mFilename;
};

} // namespace rawspeed
