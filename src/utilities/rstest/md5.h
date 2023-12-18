/*
 * MD5 hash in C and x86 assembly
 *
 * Copyright (c) 2016 Project Nayuki
 * https://www.nayuki.io/page/fast-md5-hash-implementation-in-x86-assembly
 *
 * (MIT License)
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of
 * the Software, and to permit persons to whom the Software is furnished to do
 * so,
 * subject to the following conditions:
 * - The above copyright notice and this permission notice shall be included in
 *   all copies or substantial portions of the Software.
 * - The Software is provided "as is", without warranty of any kind, express or
 *   implied, including but not limited to the warranties of merchantability,
 *   fitness for a particular purpose and noninfringement. In no event shall the
 *   authors or copyright holders be liable for any claim, damages or other
 *   liability, whether in an action of contract, tort or otherwise, arising
 * from,
 *   out of or in connection with the Software or the use or other dealings in
 * the
 *   Software.
 */

#pragma once

#include "rawspeedconfig.h"
#include "adt/Invariant.h"
#include "common/Common.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace rawspeed::md5 {

class MD5Hasher final {
public:
  using state_type = std::array<uint32_t, 4>;

  static constexpr int block_size = 64;
  using block_type = std::array<uint8_t, block_size>;

  MD5Hasher() noexcept = delete;
  ~MD5Hasher() noexcept = delete;

  MD5Hasher(const MD5Hasher&) = delete;
  MD5Hasher(MD5Hasher&&) noexcept = delete;
  MD5Hasher& operator=(const MD5Hasher&) = delete;
  MD5Hasher& operator=(MD5Hasher&&) noexcept = delete;

  static state_type compress(state_type state,
                             const block_type& block) noexcept;
};

class MD5 final {
  MD5Hasher::block_type block;
  int block_length;
  int bytes_total;

  MD5Hasher::state_type state;

  static constexpr const MD5Hasher::state_type md5_init = {
      {UINT32_C(0x67452301), UINT32_C(0xEFCDAB89), UINT32_C(0x98BADCFE),
       UINT32_C(0x10325476)}};

  void reset() noexcept {
    state = md5_init;
    block_length = 0;
    bytes_total = 0;
  }

  [[nodiscard]] int bytesAvaliableInBlock() const noexcept RAWSPEED_READONLY {
    return MD5Hasher::block_size - block_length;
  }

  [[nodiscard]] bool blockIsEmpty() const noexcept RAWSPEED_READONLY {
    return bytesAvaliableInBlock() == MD5Hasher::block_size;
  }

  [[nodiscard]] bool blockIsFull() const noexcept RAWSPEED_READONLY {
    return bytesAvaliableInBlock() == 0;
  }

  __attribute__((always_inline)) inline void compressFullBlock() noexcept;

  template <typename T>
    requires std::is_same_v<T, uint8_t>
  MD5& operator<<(const T& v) noexcept;

  template <typename T>
    requires std::is_same_v<T, uint8_t>
  MD5& take_full_block(const T* message, size_t len) noexcept;

  template <typename T>
    requires std::is_same_v<T, uint8_t>
  MD5& take_block(const T* message, size_t len) noexcept;

public:
  MD5() noexcept { reset(); }
  ~MD5() noexcept { invariant(bytes_total == 0); }

  MD5(const MD5&) = delete;
  MD5(MD5&&) noexcept = delete;
  MD5& operator=(const MD5&) = delete;
  MD5& operator=(MD5&&) noexcept = delete;

  template <typename T>
    requires std::is_same_v<T, uint8_t>
  MD5& take(const T* message, size_t len) noexcept;

  MD5Hasher::state_type flush() noexcept;
};

__attribute__((always_inline)) inline void MD5::compressFullBlock() noexcept {
  invariant(blockIsFull() && "Bad block size.");

  state = MD5Hasher::compress(state, block);
  block_length = 0;
}

template <typename T>
  requires std::is_same_v<T, uint8_t>
__attribute__((always_inline)) inline MD5&
MD5::operator<<(const T& v) noexcept {
  constexpr int numBytesToProcess = sizeof(T);

  if (blockIsFull())
    compressFullBlock();

  invariant(bytesAvaliableInBlock() >= numBytesToProcess);
  std::memcpy(block.begin() + block_length,
              reinterpret_cast<const std::byte*>(&v), numBytesToProcess);
  block_length += numBytesToProcess;
  bytes_total += numBytesToProcess;

  return *this;
}

template <typename T>
  requires std::is_same_v<T, uint8_t>
__attribute__((always_inline)) inline MD5&
MD5::take_block(const T* message, size_t len) noexcept {
  invariant(!blockIsFull());
  invariant(message != nullptr);
  invariant(len != 0);
  invariant(len <= MD5Hasher::block_size);
  invariant(static_cast<int>(len) <= bytesAvaliableInBlock());

  for (size_t i = 0; i != len; ++i) {
    invariant(!blockIsFull());
    *this << message[i];
  }

  return *this;
}

template <typename T>
  requires std::is_same_v<T, uint8_t>
__attribute__((always_inline)) inline MD5&
MD5::take_full_block(const T* message, size_t len) noexcept {
  invariant(blockIsEmpty());

  invariant(message != nullptr);
  invariant(len == MD5Hasher::block_size);

  take_block(message, len);
  invariant(blockIsFull());

  return *this;
}

template <typename T>
  requires std::is_same_v<T, uint8_t>
__attribute__((always_inline)) inline MD5& MD5::take(const T* message,
                                                     size_t len) noexcept {
  invariant(message != nullptr);
  invariant(!blockIsFull());

  if (len == 0)
    return *this;

  if (!blockIsEmpty()) {
    size_t prefix_size = std::min<size_t>(len, bytesAvaliableInBlock());
    take_block(message, prefix_size);
    message += prefix_size;
    len -= prefix_size;
    if (blockIsFull())
      compressFullBlock();
  }

  if (len == 0)
    return *this;

  const auto numFullBlocks =
      len / MD5Hasher::block_size; // Truncating division!
  for (uint64_t blockIdx = 0; blockIdx != numFullBlocks; ++blockIdx) {
    uint64_t blockBegin = MD5Hasher::block_size * blockIdx;
    take_full_block(message + blockBegin, MD5Hasher::block_size);
    compressFullBlock();
  }

  if (size_t lenRemainder = len % MD5Hasher::block_size) {
    invariant(blockIsEmpty());
    invariant(lenRemainder < MD5Hasher::block_size);
    take_block(message + (len - lenRemainder), lenRemainder);
    invariant(!blockIsFull());
  }

  return *this;
}

__attribute__((always_inline)) inline MD5Hasher::state_type
MD5::flush() noexcept {
  invariant(!blockIsFull());

  block[block_length] = 0x80;
  block_length++;

  memset(&block[block_length], 0, bytesAvaliableInBlock());
  if (bytesAvaliableInBlock() < 8) {
    block_length = 64;
    compressFullBlock();
    memset(&block[block_length], 0, bytesAvaliableInBlock());
  }

  block[64 - 8] = static_cast<uint8_t>((bytes_total & 0x1FU) << 3);
  bytes_total >>= 5;
  for (size_t i = 1; i < 8; i++) {
    block[64 - 8 + i] = static_cast<uint8_t>(bytes_total);
    bytes_total >>= 8;
  }
  block_length = 64;
  compressFullBlock();

  MD5Hasher::state_type tmp = state;
  reset();
  return tmp;
}

// computes hash of the buffer message with length len
[[nodiscard]] MD5Hasher::state_type md5_hash(const uint8_t* message,
                                             size_t len) noexcept;

// returns hash as string
[[nodiscard]] std::string
hash_to_string(const MD5Hasher::state_type& hash) noexcept;

} // namespace rawspeed::md5
