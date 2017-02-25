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

#pragma once

#include "common/Common.h" // for uchar8, uint32, uint64
#include "io/IOException.h"// for ThrowIOE
#include "io/Endianness.h" // for getByteSwapped
#include <algorithm>       // for swap

namespace RawSpeed {

// This allows to specify the nuber of bytes that each Buffer needs to
// allocate additionally to be able to remove one runtime bounds check
// in BitSream::fill. There are two options:
// 0 : allocate exactly as much data as required
// 4 : add minimum number of bytes to keep all BitStream implementations happy
//     this disables the bounds checking, saves about 1% on modern CPUs
#define BUFFER_PADDING 0UL

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

  // allocates the databuffer, and returns owning non-const pointer.
  static uchar8* Create(size_type size);

  // constructs an empty buffer
  Buffer() = default;
  // Allocates the memory
  Buffer(size_type size);
  // Data already allocated
  explicit Buffer(const uchar8 *data_, size_type size_)
      : data(data_), size(size_) {}
  // creates a (non-owning) copy / view of rhs
  Buffer(const Buffer& rhs)
    : data(rhs.data), size(rhs.size) {}
  // Move data and ownership from rhs to this
  Buffer(Buffer&& rhs) noexcept
    : data(rhs.data), size(rhs.size), isOwner(rhs.isOwner) { rhs.isOwner = false; }
  // Frees memory if owned
  ~Buffer();
  Buffer& operator=(const Buffer& rhs);

  Buffer getSubView(size_type offset, size_type size_) const {
    return Buffer(getData(offset, size_), size_);
  }
  Buffer getSubView(size_type offset) const {
    size_type newSize = size - offset;
    return Buffer(getData(offset, newSize), newSize);
  }

  // get pointer to memory at 'offset', make sure at least 'count' bytes are accessable
  const uchar8* getData(size_type offset, size_type count) const {
    if (!isValid(offset, count))
      ThrowIOE("Buffer overflow: image file may be truncated");

    return &data[offset];
  }

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
    return getByteSwapped<T>(getData(offset + index*(size_type)sizeof(T), (size_type)sizeof(T)), !inNativeByteOrder);
  }

  inline size_type getSize() const {
    return size;
  }

  inline bool isValid(size_type offset, size_type count = 1) const {
    return (uint64)offset + count - 1 < (uint64)size + BUFFER_PADDING;
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
  DataBuffer(const Buffer &data_, bool inNativeByteOrder_ = true)
      : Buffer(data_), inNativeByteOrder(inNativeByteOrder_) {}

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
