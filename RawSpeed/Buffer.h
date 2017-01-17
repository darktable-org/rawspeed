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

    http://www.klauspost.com
*/

#pragma once

#include "FileIOException.h"
#include "IOException.h"

namespace RawSpeed {

// All file maps have this much space extra at the end. This is useful for
// BitPump* that needs to have a bit of extra space at the end to be able to
// do more efficient reads without crashing
#define FILEMAP_MARGIN 16

/*************************************************************************
 * This is the buffer abstaction.
 *
 * It allows access to some piece of memory, typically a whole or part
 * of a raw file. The underlying memory may be owned by the buffer or not.
 * It supports move operations to properly deal with owneship transfer.
 * It intentionally supports only read/const access to the underlying memory.
 *
 *************************************************************************/
class Buffer
{
public:
  using size_type = uint32;

  // constructs an empty buffer
  Buffer() = default;
  // Allocates the memory
  Buffer(size_type size);
  // Data already allocated
  explicit Buffer(const uchar8* data, size_type size) : data(data), size(size) {}
  // creates a (non-owning) copy / view of rhs
  Buffer(const Buffer& rhs)
    : data(rhs.data), size(rhs.size) {}
  // Move data and ownership from rhs to this
  Buffer(Buffer&& rhs) noexcept
    : data(rhs.data), size(rhs.size), isOwner(rhs.isOwner) { rhs.isOwner = false; }
  // Frees memory if owned
  ~Buffer();
  Buffer& operator=(const Buffer& rhs);

  Buffer getSubView(size_type offset, size_type size) const {
    return Buffer(getData(offset, size), size);
  }
  Buffer getSubView(size_type offset) const {
    size_type newSize = size - offset;
    return Buffer(getData(offset, newSize), newSize);
  }

  // get pointer to memory at 'offset', make sure at least 'count' bytes are accessable
  const uchar8* getData(size_type offset, size_type count) const;

  // convenience getter for single bytes
  uchar8 operator[](size_type offset) const {
    return *getData(offset, 1);
  }

  // std begin/end iterators to allow for range loop
  const uchar8* begin() const {
    return data;
  }
  const uchar8* end() const {
    return data + size;
  }

  // get memory of type T from byte offset 'offset + sizeof(T)*index' and swap byte order if required
  template<typename T> inline T get(bool inNativeByteOrder, size_type offset, size_type index = 0) const {
    return loadMem<T>(getData(offset + index*(size_type)sizeof(T), (size_type)sizeof(T)), !inNativeByteOrder);
  }

  inline size_type getSize() const {
    return size;
  }

  inline bool isValid(size_type offset) const {
    return offset < size;
  }

  inline bool isValid(size_type offset, size_type count) const {
    return (uint64)offset + count - 1 < size;
  }

//  Buffer* clone();
//  /* For testing purposes */
//  void corrupt(int errors);
//  Buffer* cloneRandomSize();

  // deprecated:
  inline uchar8* getDataWrt(size_type offset, size_type count) {
    return const_cast<uchar8*>(getData(offset, count));
  }

protected:
  const uchar8* data = nullptr;
  size_type size = 0;
  bool isOwner = false;
};

/*
 * DataBuffer is a simple extention to Buffer. It knows about the byte order
 * of its contents and can therefore provide save access to larger than
 * byte sized data, like int, float, etc.
 */
class DataBuffer : public Buffer
{
  bool inNativeByteOrder = true;
public:
  DataBuffer() = default;
  DataBuffer(const Buffer& data, bool inNativeByteOrder = true)
    : Buffer(data), inNativeByteOrder(inNativeByteOrder) {}

  // get memory of type T from byte offset 'offset + sizeof(T)*index' and swap byte order if required
  template<typename T> inline T get(size_type offset, size_type index = 0) const {
    return Buffer::get<T>(inNativeByteOrder, offset, index);
  }

  inline bool isInNativeByteOrder() const {
    return inNativeByteOrder;
  }

  inline bool setInNativeByteOrder(bool value) {
    std::swap(inNativeByteOrder, value);
    return value;
  }
};

} // namespace RawSpeed
