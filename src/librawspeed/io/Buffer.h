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

#include "rawspeedconfig.h"
#include "adt/Array1DRef.h"
#include "adt/Casts.h"
#include "io/Endianness.h"
#include "io/IOException.h"
#include <cassert>
#include <cstdint>
#include <utility>

#ifndef NDEBUG
#include "AddressSanitizer.h"
#endif

namespace rawspeed {

/*************************************************************************
 * This is the buffer abstraction.
 *
 * It allows access to some piece of memory, typically a whole or part
 * of a raw file. The underlying memory is never owned by the buffer.
 * It intentionally supports only read/const access to the underlying memory.
 *
 *************************************************************************/
class Buffer {
public:
  using size_type = uint32_t;

protected:
  const uint8_t* data = nullptr;

private:
  size_type size = 0;

public:
  Buffer() = default;

  // NOLINTNEXTLINE(google-explicit-constructor)
  Buffer(Array1DRef<const uint8_t> data_)
      : data(data_.begin()), size(data_.size()) {
    assert(data);
    assert(!ASan::RegionIsPoisoned(data, size));
  }

  explicit Buffer(const uint8_t* data_, size_type size_)
      : Buffer(Array1DRef(data_, implicit_cast<int>(size_))) {}

  [[nodiscard]] Array1DRef<const uint8_t> getAsArray1DRef() const {
    return {data, implicit_cast<int>(size)};
  }

  explicit operator Array1DRef<const uint8_t>() const {
    return getAsArray1DRef();
  }

  [[nodiscard]] Buffer getSubView(size_type offset, size_type size_) const {
    if (!isValid(offset, size_))
      ThrowIOE("Buffer overflow: image file may be truncated");

    return getAsArray1DRef().getCrop(offset, size_).getAsArray1DRef();
  }

  [[nodiscard]] Buffer getSubView(size_type offset) const {
    if (!isValid(0, offset))
      ThrowIOE("Buffer overflow: image file may be truncated");

    size_type newSize = getSize() - offset;
    return getSubView(offset, newSize);
  }

  // convenience getter for single bytes
  uint8_t operator[](size_type offset) const {
    return getAsArray1DRef()(offset);
  }

  // std begin/end iterators to allow for range loop
  [[nodiscard]] const uint8_t* begin() const {
    return getAsArray1DRef().begin();
  }
  [[nodiscard]] const uint8_t* end() const { return getAsArray1DRef().end(); }

  // get memory of type T from byte offset 'offset + sizeof(T)*index' and swap
  // byte order if required
  template <typename T>
  [[nodiscard]] T get(bool inNativeByteOrder, size_type offset,
                      size_type index = 0) const {
    const Buffer buf =
        getSubView(offset + index * static_cast<size_type>(sizeof(T)),
                   static_cast<size_type>(sizeof(T)));
    return getByteSwapped<T>(buf.begin(), !inNativeByteOrder);
  }

  [[nodiscard]] size_type RAWSPEED_READONLY getSize() const { return size; }

  [[nodiscard]] bool isValid(size_type offset, size_type count = 1) const {
    return static_cast<uint64_t>(offset) + count <=
           static_cast<uint64_t>(getSize());
  }
};

// WARNING: both buffers must belong to the same allocation, else this is UB!
inline bool operator<(Buffer lhs, Buffer rhs) {
  return std::pair(lhs.begin(), lhs.end()) < std::pair(rhs.begin(), rhs.end());
}

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

  explicit DataBuffer(Buffer data_, Endianness endianness_)
      : Buffer(data_), endianness(endianness_) {}

  // get memory of type T from byte offset 'offset + sizeof(T)*index' and swap
  // byte order if required
  template <typename T>
  [[nodiscard]] T get(size_type offset, size_type index = 0) const {
    assert(Endianness::unknown != endianness);
    assert(Endianness::little == endianness || Endianness::big == endianness);

    return Buffer::get<T>(getHostEndianness() == endianness, offset, index);
  }

  [[nodiscard]] Endianness getByteOrder() const { return endianness; }

  Endianness setByteOrder(Endianness endianness_) {
    std::swap(endianness, endianness_);
    return endianness_;
  }
};

} // namespace rawspeed
