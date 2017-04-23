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

#include "common/Common.h"  // for uchar8, int32, uint32, ushort16, roundUp
#include "common/Memory.h"  // for alignedMalloc
#include "io/Buffer.h"      // for Buffer::size_type, Buffer, DataBuffer
#include "io/IOException.h" // for IOException (ptr only), ThrowIOE
#include <cassert>          // for assert
#include <cstring>          // for memcmp, memcpy

namespace rawspeed {

class ByteStream : public DataBuffer
{
protected:
  size_type pos = 0; // position of stream in bytes (this is next byte to deliver)

public:
  ByteStream() = default;
  explicit ByteStream(const DataBuffer& buffer) : DataBuffer(buffer) {}
  ByteStream(const Buffer &buffer, size_type offset, size_type size_,
             bool inNativeByteOrder_ = true)
      : DataBuffer(buffer.getSubView(0, offset + size_), inNativeByteOrder_),
        pos(offset) {}
  ByteStream(const Buffer &buffer, size_type offset,
             bool inNativeByteOrder_ = true)
      : DataBuffer(buffer, inNativeByteOrder_), pos(offset) {}

  // deprecated:
  ByteStream(const Buffer* f, size_type offset, size_type size_,
             bool inNativeByteOrder_ = true)
      : ByteStream(*f, offset, size_, inNativeByteOrder_) {}
  ByteStream(const Buffer* f, size_type offset, bool inNativeByteOrder_ = true)
      : ByteStream(*f, offset, inNativeByteOrder_) {}

  // return ByteStream that starts at given offset
  // i.e. this->data + offset == getSubStream(offset).data
  ByteStream getSubStream(size_type offset, size_type size_) const {
    return ByteStream(getSubView(offset, size_), 0, isInNativeByteOrder());
  }

  inline void check(size_type bytes) const {
    if (static_cast<uint64>(pos) + bytes > size)
      ThrowIOE("Out of bounds access in ByteStream");
  }

  inline size_type getPosition() const { return pos; }
  inline void setPosition(size_type newPos) {
    pos = newPos;
    check(0);
  }
  inline size_type getRemainSize() const { return size-pos; }
  inline const uchar8* peekData(size_type count) const {
    return Buffer::getData(pos, count);
  }
  inline const uchar8* getData(size_type count) {
    const uchar8* ret = Buffer::getData(pos, count);
    pos += count;
    return ret;
  }
  inline Buffer getBuffer(size_type size_) {
    Buffer ret = getSubView(pos, size_);
    pos += size_;
    return ret;
  }

  inline uchar8 peekByte(size_type i = 0) const {
    check(i+1);
    return data[pos+i];
  }

  inline void skipBytes(size_type nbytes) {
    pos += nbytes;
    check(0);
  }

  inline bool hasPatternAt(const char *pattern, size_type size_,
                           size_type relPos) const {
    if (!isValid(pos + relPos, size_))
      return false;
    return memcmp(&data[pos + relPos], pattern, size_) == 0;
  }

  inline bool hasPrefix(const char *prefix, size_type size_) const {
    return hasPatternAt(prefix, size_, 0);
  }

  inline bool skipPrefix(const char *prefix, size_type size_) {
    bool has_prefix = hasPrefix(prefix, size_);
    if (has_prefix)
      pos += size_;
    return has_prefix;
  }

  inline uchar8 getByte() {
    check(1);
    return data[pos++];
  }

  template<typename T> inline T peek(size_type i = 0) const {
    return DataBuffer::get<T>(pos, i);
  }

  template<typename T> inline T get() {
    auto ret = peek<T>();
    pos += sizeof(T);
    return ret;
  }

  inline ushort16 getU16() { return get<ushort16>(); }
  inline int32 getI32() { return get<int32>(); }
  inline uint32 getU32() { return get<uint32>(); }
  inline float getFloat() { return get<float>(); }

  const char* peekString() const {
    if (memchr(peekData(getRemainSize()), 0, getRemainSize()) == nullptr)
      ThrowIOE("String is not null-terminated");
    return reinterpret_cast<const char*>(&data[pos]);
  }

  // Increments the stream to after the next zero byte and returns the bytes in between (not a copy).
  // If the first byte is zero, stream is incremented one.
  const char* getString() {
    size_type start = pos;
    do {
      check(1);
    } while (data[pos++] != 0);
    return reinterpret_cast<const char*>(&data[start]);
  }

  // recalculate the internal data/position information such that current position
  // i.e. getData() before == getData() after but getPosition() after == newPosition
  // this is only used for DNGPRIVATEDATA handling to restore the original offset
  // in case the private data / maker note has been moved within in the file
  // TODO: could add a lower bound check later if required.
  void rebase(const size_type newPosition, const size_type newSize) {
    // does that pair (position, size) make sense for this buffer? may throw
    const uchar8* const dataRebaseCheck __attribute__((unused)) =
        Buffer::getData(newPosition, newSize);

    const uchar8* dataAtNewPosition = getData(newSize);
    data = dataAtNewPosition - newPosition;
    size = newPosition + newSize;

    // buffer sanity self-check. should not throw, unless there is a mistake
    const uchar8* const rebasedCheck __attribute__((unused)) = peekData(size);

    // check that all the assumptions still hold, and we rebased correctly
    assert(getPosition() == newPosition);
    assert(getSize() == newSize);
    assert(dataRebaseCheck == rebasedCheck);
  }

  // special factory function to set up internal buffer with copy of passed data.
  // only necessary to create 'fake' TiffEntries (see e.g. RAF)
  static ByteStream createCopy(void* data, size_type size) {
    ByteStream bs;
    auto* new_data = alignedMalloc<uchar8, 8>(roundUp(size, 8));
    memcpy(new_data, data, size);
    bs.data = new_data;
    bs.size = size;
    bs.isOwner = true;
    return bs; // hint: copy elision or move will happen
  }
};

} // namespace rawspeed
