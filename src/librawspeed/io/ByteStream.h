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
#include "adt/Casts.h"
#include "adt/Invariant.h"
#include "io/Buffer.h"
#include "io/IOException.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iterator>
#include <limits>
#include <string_view>

#ifndef NDEBUG
#include "AddressSanitizer.h"
#endif

namespace rawspeed {

class ByteStream final : public DataBuffer {
  size_type pos =
      0; // position of stream in bytes (this is next byte to deliver)

public:
  ByteStream() = default;

  explicit ByteStream(DataBuffer buffer) : DataBuffer(buffer) {}

  // return ByteStream that starts at given offset
  // i.e. this->data + offset == getSubStream(offset).data
  [[nodiscard]] ByteStream getSubStream(size_type offset,
                                        size_type size_) const {
    return ByteStream(DataBuffer(getSubView(offset, size_), getByteOrder()));
  }

  [[nodiscard]] ByteStream getSubStream(size_type offset) const {
    return ByteStream(DataBuffer(getSubView(offset), getByteOrder()));
  }

  [[nodiscard]] size_type check(size_type bytes) const {
    if (!isValid(pos, bytes))
      ThrowIOE("Out of bounds access in ByteStream");
    [[maybe_unused]] Buffer tmp = getSubView(pos, bytes);
    assert(tmp.getSize() == bytes);
    assert(!ASan::RegionIsPoisoned(tmp.begin(), tmp.getSize()));
    return bytes;
  }

  [[nodiscard]] size_type check(size_type nmemb, size_type size_) const {
    if (size_ && nmemb > std::numeric_limits<size_type>::max() / size_)
      ThrowIOE("Integer overflow when calculating stream length");
    return check(nmemb * size_);
  }

  [[nodiscard]] size_type getPosition() const {
    invariant(getSize() >= pos);
    (void)check(0);
    return pos;
  }
  void setPosition(size_type newPos) {
    pos = newPos;
    (void)check(0);
  }
  [[nodiscard]] size_type RAWSPEED_READONLY getRemainSize() const {
    invariant(getSize() >= pos);
    (void)check(0);
    return getSize() - pos;
  }
  [[nodiscard]] const uint8_t* peekData(size_type count) const {
    return Buffer::getSubView(pos, count).begin();
  }
  const uint8_t* getData(size_type count) {
    const uint8_t* ret = peekData(count);
    pos += count;
    return ret;
  }
  [[nodiscard]] Buffer peekBuffer(size_type size_) const {
    return getSubView(pos, size_);
  }
  Buffer getBuffer(size_type size_) {
    Buffer ret = peekBuffer(size_);
    pos += size_;
    return ret;
  }
  [[nodiscard]] Buffer peekRemainingBuffer() const {
    return getSubView(pos, getRemainSize());
  }
  [[nodiscard]] ByteStream peekStream(size_type size_) const {
    return getSubStream(pos, size_);
  }
  [[nodiscard]] ByteStream peekStream(size_type nmemb, size_type size_) const {
    if (size_ && nmemb > std::numeric_limits<size_type>::max() / size_)
      ThrowIOE("Integer overflow when calculating stream length");
    return peekStream(nmemb * size_);
  }
  ByteStream getStream(size_type size_) {
    ByteStream ret = peekStream(size_);
    pos += size_;
    return ret;
  }
  ByteStream getStream(size_type nmemb, size_type size_) {
    if (size_ && nmemb > std::numeric_limits<size_type>::max() / size_)
      ThrowIOE("Integer overflow when calculating stream length");
    return getStream(nmemb * size_);
  }

  void skipBytes(size_type nbytes) { pos += check(nbytes); }
  void skipBytes(size_type nmemb, size_type size_) {
    pos += check(nmemb, size_);
  }

  [[nodiscard]] bool hasPatternAt(std::string_view pattern,
                                  size_type relPos) const {
    if (!isValid(pos + relPos, implicit_cast<size_type>(pattern.size())))
      return false;
    auto tmp =
        getSubView(pos + relPos, implicit_cast<size_type>(pattern.size()));
    assert(tmp.getSize() == pattern.size());
    return std::equal(tmp.begin(), tmp.end(), pattern.begin());
  }

  [[nodiscard]] bool hasPrefix(std::string_view prefix) const {
    return hasPatternAt(prefix, /*relPos=*/0);
  }

  bool skipPrefix(std::string_view prefix) {
    bool has_prefix = hasPrefix(prefix);
    if (has_prefix)
      pos += prefix.size();
    return has_prefix;
  }

  template <typename T> [[nodiscard]] T peek(size_type i = 0) const {
    return DataBuffer::get<T>(pos, i);
  }
  template <typename T> T get() {
    auto ret = peek<T>();
    pos += sizeof(T);
    return ret;
  }

  [[nodiscard]] uint8_t peekByte(size_type i = 0) const {
    return peek<uint8_t>(i);
  }
  uint8_t getByte() { return get<uint8_t>(); }

  [[nodiscard]] uint16_t peekU16() const { return peek<uint16_t>(); }

  [[nodiscard]] uint32_t peekU32(size_type i = 0) const {
    return peek<uint32_t>(i);
  }

  uint16_t getU16() { return get<uint16_t>(); }
  int32_t getI32() { return get<int32_t>(); }
  uint32_t getU32() { return get<uint32_t>(); }
  float getFloat() { return get<float>(); }

  [[nodiscard]] std::string_view peekString() const {
    Buffer tmp = peekBuffer(getRemainSize());
    const auto* termIter = std::find(tmp.begin(), tmp.end(), '\0');
    if (termIter == tmp.end())
      ThrowIOE("String is not null-terminated");
    std::string_view::size_type strlen = std::distance(tmp.begin(), termIter);
    return {reinterpret_cast<const char*>(tmp.begin()), strlen};
  }

  // Increments the stream to after the next zero byte and returns the bytes in
  // between (not a copy). If the first byte is zero, stream is incremented one.
  [[nodiscard]] std::string_view getString() {
    std::string_view str = peekString();
    skipBytes(implicit_cast<Buffer::size_type>(1 + str.size()));
    return str;
  }
};

} // namespace rawspeed
