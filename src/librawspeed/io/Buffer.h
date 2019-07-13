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

#include "AddressSanitizer.h" // for ASan
#include "common/Common.h"    // for uint8_t, uint64_t, uint32
#include "common/Memory.h"    // for alignedFree
#include "io/Endianness.h"    // for Endianness, Endianness::little, getHos...
#include "io/IOException.h"   // for ThrowIOE
#include <cassert>            // for assert
#include <memory>             // for unique_ptr
#include <utility>            // for swap

namespace rawspeed {

// This allows to specify the number of bytes that each Buffer needs to
// allocate additionally to be able to remove one runtime bounds check
// in BitStream::fill. There are two sane choices:
// 0 : allocate exactly as much data as required, or
// set it to the value of  BitStreamCacheBase::MaxProcessBytes
#define BUFFER_PADDING 0UL

// if the padding is >= 4, bounds checking in BitStream::fill are not compiled,
// which supposedly saves about 1% on modern CPUs
// WARNING: if the padding is >= 4, do *NOT* create Buffer from
// passed unowning pointer and size. Or, subtract BUFFER_PADDING from size.
// else bound checks will malfunction => bad things can happen !!!

/*************************************************************************
 * This is the buffer abstraction.
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

protected:
  const uint8_t* data = nullptr;
  size_type size = 0;
  bool isOwner = false;

public:
  // allocates the databuffer, and returns owning non-const pointer.
  static std::unique_ptr<uint8_t, decltype(&alignedFree)>
  Create(size_type size);

  // constructs an empty buffer
  Buffer() = default;

  // Allocates the memory
  explicit Buffer(size_type size_) : Buffer(Create(size_), size_) {
    assert(!ASan::RegionIsPoisoned(data, size));
  }

  // creates buffer from owning unique_ptr
  Buffer(std::unique_ptr<uint8_t, decltype(&alignedFree)> data_,
         size_type size_);

  // Data already allocated
  explicit Buffer(const uint8_t* data_, size_type size_)
      : data(data_), size(size_) {
    static_assert(BUFFER_PADDING == 0, "please do make sure that you do NOT "
                                       "call this function from YOUR code, and "
                                       "then comment-out this assert.");
    assert(!ASan::RegionIsPoisoned(data, size));
  }

  // creates a (non-owning) copy / view of rhs
  Buffer(const Buffer& rhs) : data(rhs.data), size(rhs.size) {
    assert(!ASan::RegionIsPoisoned(data, size));
  }

  // Move data and ownership from rhs to this
  Buffer(Buffer&& rhs) noexcept
      : data(rhs.data), size(rhs.size), isOwner(rhs.isOwner) {
    assert(!ASan::RegionIsPoisoned(data, size));
    rhs.isOwner = false;
  }

  // Frees memory if owned
  ~Buffer();

  Buffer& operator=(Buffer&& rhs) noexcept;
  Buffer& operator=(const Buffer& rhs);

  Buffer getSubView(size_type offset, size_type size_) const {
    if (!isValid(0, offset))
      ThrowIOE("Buffer overflow: image file may be truncated");

    return Buffer(getData(offset, size_), size_);
  }

  Buffer getSubView(size_type offset) const {
    if (!isValid(0, offset))
      ThrowIOE("Buffer overflow: image file may be truncated");

    size_type newSize = size - offset;
    return getSubView(offset, newSize);
  }

  // get pointer to memory at 'offset', make sure at least 'count' bytes are accessible
  const uint8_t* getData(size_type offset, size_type count) const {
    if (!isValid(offset, count))
      ThrowIOE("Buffer overflow: image file may be truncated");

    assert(data);
    assert(!ASan::RegionIsPoisoned(data + offset, count));

    return data + offset;
  }

  // convenience getter for single bytes
  uint8_t operator[](size_type offset) const { return *getData(offset, 1); }

  // std begin/end iterators to allow for range loop
  const uint8_t* begin() const {
    assert(data);
    assert(!ASan::RegionIsPoisoned(data, 0));
    return data;
  }
  const uint8_t* end() const {
    assert(data);
    assert(!ASan::RegionIsPoisoned(data, size));
    return data + size;
  }

  // get memory of type T from byte offset 'offset + sizeof(T)*index' and swap byte order if required
  template<typename T> inline T get(bool inNativeByteOrder, size_type offset, size_type index = 0) const {
    return getByteSwapped<T>(
        getData(offset + index * static_cast<size_type>(sizeof(T)),
                static_cast<size_type>(sizeof(T))),
        !inNativeByteOrder);
  }

  inline size_type getSize() const {
    assert(!ASan::RegionIsPoisoned(data, size));
    return size;
  }

  inline bool isValid(size_type offset, size_type count = 1) const {
    return static_cast<uint64_t>(offset) + count <=
           static_cast<uint64_t>(size) + BUFFER_PADDING;
  }

//  Buffer* clone();
//  /* For testing purposes */
//  void corrupt(int errors);
//  Buffer* cloneRandomSize();
};

/*
 * DataBuffer is a simple extension to Buffer. It knows about the byte order
 * of its contents and can therefore provide save access to larger than
 * byte sized data, like int, float, etc.
 */
class DataBuffer : public Buffer {
  // FIXME: default should be Endianness::unknown !

  Endianness endianness = Endianness::little;

public:
  DataBuffer() = default;

  explicit DataBuffer(const Buffer& data_, Endianness endianness_)
      : Buffer(data_), endianness(endianness_) {}

  // get memory of type T from byte offset 'offset + sizeof(T)*index' and swap
  // byte order if required
  template <typename T>
  inline T get(size_type offset, size_type index = 0) const {
    assert(Endianness::unknown != endianness);
    assert(Endianness::little == endianness || Endianness::big == endianness);

    return Buffer::get<T>(getHostEndianness() == endianness, offset, index);
  }

  inline Endianness getByteOrder() const { return endianness; }

  inline Endianness setByteOrder(Endianness endianness_) {
    std::swap(endianness, endianness_);
    return endianness_;
  }
};

} // namespace rawspeed
