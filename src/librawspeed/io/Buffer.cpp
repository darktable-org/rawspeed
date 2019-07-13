/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2017 Axel Waggershauser

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

#include "io/Buffer.h"
#include "AddressSanitizer.h" // for ASan
#include "common/Common.h"    // for uint8_t, roundUp
#include "common/Memory.h"    // for alignedFree, alignedFreeConstPtr, alig...
#include "io/IOException.h"   // for ThrowIOE
#include <cassert>            // for assert
#include <memory>             // for unique_ptr

using std::unique_ptr;

namespace rawspeed {

unique_ptr<uint8_t, decltype(&alignedFree)> Buffer::Create(size_type size) {
  if (!size)
    ThrowIOE("Trying to allocate 0 bytes sized buffer.");

  unique_ptr<uint8_t, decltype(&alignedFree)> data(
      alignedMalloc<uint8_t, 16>(roundUp(size + BUFFER_PADDING, 16)),
      &alignedFree);
  if (!data)
    ThrowIOE("Failed to allocate %uz bytes memory buffer.", size);

  assert(!ASan::RegionIsPoisoned(data.get(), size));

  return data;
}

Buffer::Buffer(unique_ptr<uint8_t, decltype(&alignedFree)> data_,
               size_type size_)
    : size(size_) {
  if (!size)
    ThrowIOE("Buffer has zero size?");

  if (data_.get_deleter() != &alignedFree)
    ThrowIOE("Wrong deleter. Expected rawspeed::alignedFree()");

  data = data_.release();
  if (!data)
    ThrowIOE("Memory buffer is nonexistent");

  assert(!ASan::RegionIsPoisoned(data, size));

  isOwner = true;
}

Buffer::~Buffer() {
  if (isOwner) {
    alignedFreeConstPtr(data);
  }
}

Buffer& Buffer::operator=(Buffer&& rhs) noexcept {
  if (this == &rhs) {
    assert(!ASan::RegionIsPoisoned(data, size));
    return *this;
  }

  if (isOwner)
    alignedFreeConstPtr(data);

  data = rhs.data;
  size = rhs.size;
  isOwner = rhs.isOwner;

  assert(!ASan::RegionIsPoisoned(data, size));

  rhs.isOwner = false;

  return *this;
}

Buffer& Buffer::operator=(const Buffer& rhs) {
  if (this == &rhs) {
    assert(!ASan::RegionIsPoisoned(data, size));
    return *this;
  }

  Buffer unOwningTmp(rhs.data, rhs.size);
  *this = std::move(unOwningTmp);
  assert(!isOwner);
  assert(!ASan::RegionIsPoisoned(data, size));

  return *this;
}

#if 0
Buffer* Buffer::clone() {
  Buffer *new_map = new Buffer(size);
  memcpy(new_map->data, data, size);
  return new_map;
}

Buffer* Buffer::cloneRandomSize() {
  uint32 new_size = (rand() | (rand() << 15)) % size;
  Buffer *new_map = new Buffer(new_size);
  memcpy(new_map->data, data, new_size);
  return new_map;
}

void Buffer::corrupt(int errors) {
  for (int i = 0; i < errors; i++) {
    uint32 pos = (rand() | (rand() << 15)) % size;
    data[pos] = rand() & 0xff;
  }
}
#endif

} // namespace rawspeed
