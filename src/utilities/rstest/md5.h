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
#include "adt/Array1DRef.h"
#include "adt/Casts.h"
#include "adt/CroppedArray1DRef.h" // IWYU pragma: keep
#include "adt/Invariant.h"
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdint.h>
#include <string>
#include <type_traits>
#include <variant>

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
                             Array1DRef<const uint8_t> block) noexcept;
};

template <class> inline constexpr bool always_false_v = false;

template <int N> class BufferCoalescer final {
  struct NoBuffer final {
    static constexpr int block_length = 0;
  };
  struct FullBufferRef final {
    // NOLINTNEXTLINE(google-explicit-constructor)
    FullBufferRef(Array1DRef<const uint8_t> block_) : block(block_.begin()) {
      invariant(block_.size() == block_length);
    }
    const uint8_t* block;
    static constexpr int block_length = N;
  };
  struct CoalescingBuffer final {
    MD5Hasher::block_type block;
    int block_length = 0;
  };

  std::variant<NoBuffer, FullBufferRef, CoalescingBuffer> state = NoBuffer();

public:
  template <typename ArgTy>
  void take_block_impl(ArgTy& arg,
                       Array1DRef<const uint8_t> message) const = delete;

private:
  template <typename ArgTy>
    requires std::same_as<ArgTy, NoBuffer>
  void take_block_impl(NoBuffer& /*unused*/,
                       Array1DRef<const uint8_t> message) {
    invariant(message.size() != 0);

    if (message.size() == N) {
      state = FullBufferRef(message);
      return;
    }

    CoalescingBuffer buf;
    take_block_impl<CoalescingBuffer>(buf, message);
    state = buf;
  }

  template <typename ArgTy>
    requires std::same_as<ArgTy, CoalescingBuffer>
  void take_block_impl(CoalescingBuffer& arg,
                       Array1DRef<const uint8_t> message) const {
    invariant(message.size() != 0);
    invariant(message.size() < N);

    invariant(arg.block_length + message.size() <= N);

    auto out =
        Array1DRef(arg.block.data(), implicit_cast<int>(arg.block.size()))
            .getCrop(/*offset=*/arg.block_length, /*size=*/message.size());
    std::copy(message.begin(), message.end(), out.begin());
    arg.block_length += message.size();
  }

  [[nodiscard]] __attribute__((always_inline)) int length() const noexcept {
    return std::visit([](const auto& arg) { return arg.block_length; }, state);
  }

public:
  [[nodiscard]] __attribute__((always_inline)) int
  bytesAvaliable() const noexcept {
    return N - length();
  }

  [[nodiscard]] bool blockIsEmpty() const noexcept {
    return bytesAvaliable() == N;
  }

  [[nodiscard]] bool blockIsFull() const noexcept {
    return bytesAvaliable() == 0;
  }

  void reset() noexcept { state = NoBuffer(); }

  __attribute__((always_inline)) void
  take_block(Array1DRef<const uint8_t> message) noexcept {
    invariant(message.size() != 0);
    invariant(message.size() <= N);

    std::visit(
        [this, message]<typename T>(T& arg) {
          if constexpr (std::is_same_v<T, FullBufferRef>)
            __builtin_unreachable();
          else
            this->take_block_impl<T>(arg, message);
        },
        state);
  }

  [[nodiscard]] FullBufferRef getAsFullBufferRef() const {
    return std::visit(
        []<typename T>(const T& arg) -> FullBufferRef {
          if constexpr (std::is_same_v<T, FullBufferRef>)
            return arg;
          else if constexpr (std::is_same_v<T, CoalescingBuffer>) {
            invariant(arg.block_length == N);
            return {{arg.block.data(), arg.block_length}};
          } else if constexpr (std::is_same_v<T, NoBuffer>)
            __builtin_unreachable();
          else
            static_assert(always_false_v<T>, "non-exhaustive visitor!");
        },
        state);
  }
};

class MD5 final {
  BufferCoalescer<MD5Hasher::block_size> buffer;
  int bytes_total;

  MD5Hasher::state_type state;

  static constexpr const MD5Hasher::state_type md5_init = {
      {UINT32_C(0x67452301), UINT32_C(0xEFCDAB89), UINT32_C(0x98BADCFE),
       UINT32_C(0x10325476)}};

  void reset() noexcept {
    state = md5_init;
    buffer.reset();
    bytes_total = 0;
  }

  [[nodiscard]] int bytesAvaliableInBlock() const noexcept RAWSPEED_READONLY {
    return buffer.bytesAvaliable();
  }

  [[nodiscard]] bool blockIsEmpty() const noexcept RAWSPEED_READONLY {
    return buffer.blockIsEmpty();
  }

  [[nodiscard]] bool blockIsFull() const noexcept RAWSPEED_READONLY {
    return buffer.blockIsFull();
  }

  __attribute__((always_inline)) inline void compressFullBlock() noexcept;

  template <typename T>
    requires std::is_same_v<T, uint8_t>
  MD5& operator<<(const T& v) noexcept;

  template <typename T>
    requires std::is_same_v<T, uint8_t>
  MD5& take_full_block(Array1DRef<const T> message) noexcept;

  template <typename T>
    requires std::is_same_v<T, uint8_t>
  MD5& take_block(Array1DRef<const T> message) noexcept;

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

  const auto fullBlock = buffer.getAsFullBufferRef();
  state = MD5Hasher::compress(
      state, {fullBlock.block, decltype(fullBlock)::block_length});
  buffer.reset();
}

template <typename T>
  requires std::is_same_v<T, uint8_t>
__attribute__((always_inline)) inline MD5&
MD5::take_block(Array1DRef<const T> message) noexcept {
  invariant(!blockIsFull());
  invariant(message.size() != 0);
  invariant(message.size() <= MD5Hasher::block_size);
  invariant(message.size() <= bytesAvaliableInBlock());

  buffer.take_block(message);
  bytes_total += message.size();

  return *this;
}

template <typename T>
  requires std::is_same_v<T, uint8_t>
__attribute__((always_inline)) inline MD5&
MD5::take_full_block(Array1DRef<const T> message) noexcept {
  invariant(blockIsEmpty());

  invariant(message.size() == MD5Hasher::block_size);

  take_block(message);
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

  auto msg = Array1DRef(message, implicit_cast<int>(len));

  if (!blockIsEmpty()) {
    auto prefix_size = implicit_cast<int>(
        std::min<size_t>(msg.size(), bytesAvaliableInBlock()));
    auto prefixMsg =
        msg.getCrop(/*offset=*/0, /*size=*/prefix_size).getAsArray1DRef();
    msg = msg.getCrop(prefixMsg.size(), /*size=*/msg.size() - prefixMsg.size())
              .getAsArray1DRef();
    take_block(prefixMsg);
    if (blockIsFull())
      compressFullBlock();
  }

  if (msg.size() == 0)
    return *this;

  const auto numFullBlocks = implicit_cast<int>(
      msg.size() / MD5Hasher::block_size); // Truncating division!
  for (int blockIdx = 0; blockIdx != numFullBlocks; ++blockIdx) {
    auto innerMsg = msg.getCrop(/*offset=*/0, /*size=*/MD5Hasher::block_size)
                        .getAsArray1DRef();
    msg = msg.getCrop(innerMsg.size(), /*size=*/msg.size() - innerMsg.size())
              .getAsArray1DRef();
    take_full_block(innerMsg);
    compressFullBlock();
  }

  if (msg.size() > 0) {
    invariant(blockIsEmpty());
    invariant(msg.size() < MD5Hasher::block_size);
    take_block(msg);
    invariant(!blockIsFull());
  }

  return *this;
}

__attribute__((always_inline)) inline MD5Hasher::state_type
MD5::flush() noexcept {
  invariant(!blockIsFull());

  static constexpr std::array<uint8_t, 1> magic0 = {0x80};
  buffer.take_block({magic0.data(), magic0.size()});

  static constexpr std::array<uint8_t, MD5Hasher::block_size> zeropadding = {};
  if (bytesAvaliableInBlock() < 8) {
    if (bytesAvaliableInBlock() > 0)
      buffer.take_block({zeropadding.data(), bytesAvaliableInBlock()});
    compressFullBlock();
  }

  if (bytesAvaliableInBlock() > 8)
    buffer.take_block({zeropadding.data(), bytesAvaliableInBlock() - 8});

  std::array<uint8_t, 8> magic1;
  magic1[0] = static_cast<uint8_t>((bytes_total & 0x1FU) << 3);
  bytes_total >>= 5;
  for (size_t i = 1; i < 8; i++) {
    magic1[i] = static_cast<uint8_t>(bytes_total);
    bytes_total >>= 8;
  }
  buffer.take_block({magic1.data(), magic1.size()});
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
