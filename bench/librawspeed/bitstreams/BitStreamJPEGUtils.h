/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2024 Roman Lebedev

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

#include "adt/Array1DRef.h"
#include "adt/Casts.h"
#include "adt/DefaultInitAllocatorAdaptor.h"
#include "adt/Invariant.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace rawspeed {

struct JPEGStuffedByteStreamGenerator final {
  std::vector<uint8_t,
              DefaultInitAllocatorAdaptor<uint8_t, std::allocator<uint8_t>>>
      dataStorage;
  int64_t numBytesGenerated;

  [[nodiscard]] Array1DRef<const uint8_t> getInput() const {
    return {dataStorage.data(), implicit_cast<int>(dataStorage.size())};
  }

  explicit JPEGStuffedByteStreamGenerator(int64_t numBytesMax,
                                          bool AppendStuffingByte);
};

struct NonJPEGByteStreamGenerator final {
  std::vector<uint8_t,
              DefaultInitAllocatorAdaptor<uint8_t, std::allocator<uint8_t>>>
      dataStorage;
  int64_t numBytesGenerated;

  [[nodiscard]] Array1DRef<const uint8_t> getInput() const {
    return {dataStorage.data(), implicit_cast<int>(dataStorage.size())};
  }

  __attribute__((noinline)) explicit NonJPEGByteStreamGenerator(
      const int64_t numBytesMax)
      : numBytesGenerated(numBytesMax) {
    invariant(numBytesGenerated > 0);
    dataStorage.resize(implicit_cast<size_t>(numBytesGenerated), 0x00);
  }
};

} // namespace rawspeed
