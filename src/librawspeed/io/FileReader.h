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

#include "adt/AlignedAllocator.h"
#include "adt/DefaultInitAllocatorAdaptor.h"
#include "io/Buffer.h"
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace rawspeed {

class Buffer;
template <class T, int alignment> class AlignedAllocator;

class FileReader final {
  const char* fileName;

public:
  explicit FileReader(const char* fileName_) : fileName(fileName_) {}

  [[nodiscard]] std::pair<
      std::unique_ptr<std::vector<
          uint8_t,
          DefaultInitAllocatorAdaptor<uint8_t, AlignedAllocator<uint8_t, 16>>>>,
      Buffer>
  readFile() const;
};

} // namespace rawspeed
