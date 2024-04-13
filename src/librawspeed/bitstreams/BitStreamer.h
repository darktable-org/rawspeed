/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2017 Axel Waggershauser
    Copyright (C) 2017-2021 Roman Lebedev

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
#include "adt/Invariant.h"
#include "adt/VariableLengthLoad.h"
#include "bitstreams/BitStream.h"
#include "bitstreams/BitStreamPosition.h"
#include "io/Endianness.h"
#include "io/IOException.h"
#include <array>
#include <climits>
#include <cstddef>
#include <cstdint>

namespace rawspeed {

template <typename BIT_STREAM> struct BitStreamerTraits;

template <typename Tag> struct BitStreamerReplenisherBase {
  using size_type = int32_t;

  using Traits = BitStreamerTraits<Tag>;
  using StreamTraits = BitStreamTraits<Traits::Tag>;

  Array1DRef<const std::byte> input;
  int pos = 0;

  void establishClassInvariants() const noexcept;

  BitStreamerReplenisherBase() = delete;

  explicit BitStreamerReplenisherBase(Array1DRef<const std::byte> input_)
      : input(input_) {
    if (input.size() < BitStreamerTraits<Tag>::MaxProcessBytes)
      ThrowIOE("Bit stream size is smaller than MaxProcessBytes");
  }
};

template <typename Tag>
__attribute__((always_inline)) inline void
BitStreamerReplenisherBase<Tag>::establishClassInvariants() const noexcept {
  input.establishClassInvariants();
  invariant(input.size() >= BitStreamerTraits<Tag>::MaxProcessBytes);
  invariant(pos >= 0);
  invariant(pos % StreamTraits::MinLoadStepByteMultiple == 0);
  // `pos` *could* be out-of-bounds of `input`.
}

template <typename Tag>
struct BitStreamerForwardSequentialReplenisher final
    : public BitStreamerReplenisherBase<Tag> {
  using Base = BitStreamerReplenisherBase<Tag>;
  using Traits = BitStreamerTraits<Tag>;
  using StreamTraits = BitStreamTraits<Traits::Tag>;

  using Base::BitStreamerReplenisherBase;

  BitStreamerForwardSequentialReplenisher() = delete;

  [[nodiscard]] typename Base::size_type getPos() const {
    Base::establishClassInvariants();
    return Base::pos;
  }
  [[nodiscard]] typename Base::size_type getRemainingSize() const {
    Base::establishClassInvariants();
    return Base::input.size() - getPos();
  }
  void markNumBytesAsConsumed(typename Base::size_type numBytes) {
    Base::establishClassInvariants();
    invariant(numBytes >= 0);
    invariant(numBytes != 0);
    invariant(numBytes % StreamTraits::MinLoadStepByteMultiple == 0);
    Base::pos += numBytes;
  }

  std::array<std::byte, BitStreamerTraits<Tag>::MaxProcessBytes> getInput() {
    Base::establishClassInvariants();

    std::array<std::byte, BitStreamerTraits<Tag>::MaxProcessBytes> tmpStorage;
    auto tmp = Array1DRef<std::byte>(tmpStorage.data(),
                                     implicit_cast<int>(tmpStorage.size()));

    // Do we have BitStreamerTraits<Tag>::MaxProcessBytes or more bytes left in
    // the input buffer? If so, then we can just read from said buffer.
    if (getPos() + BitStreamerTraits<Tag>::MaxProcessBytes <=
        Base::input.size()) [[likely]] {
      auto currInput =
          Base::input.getCrop(getPos(), BitStreamerTraits<Tag>::MaxProcessBytes)
              .getAsArray1DRef();
      invariant(currInput.size() == tmp.size());
      memcpy(tmp.begin(), currInput.begin(),
             BitStreamerTraits<Tag>::MaxProcessBytes);
      return tmpStorage;
    }

    // We have to use intermediate buffer, either because the input is running
    // out of bytes, or because we want to enforce bounds checking.

    // Note that in order to keep all fill-level invariants we must allow to
    // over-read past-the-end a bit.
    if (getPos() > Base::input.size() +
                       2 * BitStreamerTraits<Tag>::MaxProcessBytes) [[unlikely]]
      ThrowIOE("Buffer overflow read in BitStreamer");

    variableLengthLoadNaiveViaMemcpy(tmp, Base::input, getPos());

    return tmpStorage;
  }
};

template <typename Derived,
          typename Replenisher =
              BitStreamerForwardSequentialReplenisher<Derived>>
class BitStreamer {
public:
  using size_type = int32_t;
  using Traits = BitStreamerTraits<Derived>;
  using StreamTraits = BitStreamTraits<Traits::Tag>;

  using Cache = typename StreamTraits::StreamFlow;

protected:
  Cache cache;

private:
  Replenisher replenisher;

  // this method can be re-implemented in the concrete BitStreamer template
  // specializations. It will return the number of bytes processed. It needs
  // to process up to BitStreamerTraits<Tag>::MaxProcessBytes bytes of input.
  size_type
  fillCache(std::array<std::byte, BitStreamerTraits<Derived>::MaxProcessBytes>
                inputStorage) {
    static_assert(BitStreamCacheBase::MaxGetBits >= 32, "check implementation");
    establishClassInvariants();
    auto input = Array1DRef<std::byte>(inputStorage.data(),
                                       implicit_cast<int>(inputStorage.size()));
    invariant(input.size() == Traits::MaxProcessBytes);

    constexpr int StreamChunkBitwidth =
        bitwidth<typename StreamTraits::ChunkType>();
    static_assert(CHAR_BIT * Traits::MaxProcessBytes >= StreamChunkBitwidth);
    static_assert(CHAR_BIT * Traits::MaxProcessBytes % StreamChunkBitwidth ==
                  0);
    constexpr int NumChunksNeeded =
        (CHAR_BIT * Traits::MaxProcessBytes) / StreamChunkBitwidth;
    static_assert(NumChunksNeeded >= 1);

    for (int i = 0; i != NumChunksNeeded; ++i) {
      auto chunkInput =
          input.getBlock(sizeof(typename StreamTraits::ChunkType), i);
      auto chunk = getByteSwapped<typename StreamTraits::ChunkType>(
          chunkInput.begin(),
          StreamTraits::ChunkEndianness != getHostEndianness());
      cache.push(chunk, StreamChunkBitwidth);
    }
    return Traits::MaxProcessBytes;
  }

public:
  void establishClassInvariants() const noexcept {
    cache.establishClassInvariants();
    replenisher.establishClassInvariants();
  }

  BitStreamer() = delete;

  explicit BitStreamer(Array1DRef<const std::byte> input) : replenisher(input) {
    establishClassInvariants();
  }

  void reload() {
    establishClassInvariants();

    BitStreamPosition<Traits::Tag> state;
    state.pos = getInputPosition();
    state.fillLevel = getFillLevel();
    const auto bsPos = getAsByteStreamPosition(state);

    auto replacement = BitStreamer(replenisher.input);
    if (bsPos.bytePos != 0)
      replacement.replenisher.markNumBytesAsConsumed(bsPos.bytePos);
    replacement.fill();
    replacement.skipBitsNoFill(bsPos.numBitsToSkip);
    *this = std::move(replacement);
  }

  void fill(int nbits = Cache::MaxGetBits) {
    establishClassInvariants();
    invariant(nbits >= 0);
    invariant(nbits != 0);
    invariant(nbits <= Cache::MaxGetBits);

    if (cache.fillLevel >= nbits)
      return;

    const auto input = replenisher.getInput();
    const auto numBytes = static_cast<Derived*>(this)->fillCache(input);
    replenisher.markNumBytesAsConsumed(numBytes);
    invariant(cache.fillLevel >= nbits);
  }

  // these methods might be specialized by implementations that support it
  [[nodiscard]] size_type RAWSPEED_READONLY getInputPosition() const {
    establishClassInvariants();
    return replenisher.getPos();
  }

  // these methods might be specialized by implementations that support it
  [[nodiscard]] size_type getStreamPosition() const {
    establishClassInvariants();
    return getInputPosition() - (cache.fillLevel >> 3);
  }

  [[nodiscard]] size_type getRemainingSize() const {
    establishClassInvariants();
    return replenisher.getRemainingSize();
  }

  [[nodiscard]] size_type RAWSPEED_READONLY getFillLevel() const {
    establishClassInvariants();
    return cache.fillLevel;
  }

  uint32_t RAWSPEED_READONLY peekBitsNoFill(int nbits) {
    establishClassInvariants();
    invariant(nbits >= 0);
    invariant(nbits != 0);
    invariant(nbits <= Cache::MaxGetBits);
    return cache.peek(nbits);
  }

  void skipBitsNoFill(int nbits) {
    establishClassInvariants();
    invariant(nbits >= 0);
    // `nbits` could be zero.
    invariant(nbits <= Cache::MaxGetBits);
    cache.skip(nbits);
  }

  uint32_t getBitsNoFill(int nbits) {
    establishClassInvariants();
    invariant(nbits >= 0);
    invariant(nbits != 0);
    invariant(nbits <= Cache::MaxGetBits);
    uint32_t ret = peekBitsNoFill(nbits);
    skipBitsNoFill(nbits);
    return ret;
  }

  uint32_t peekBits(int nbits) {
    establishClassInvariants();
    invariant(nbits >= 0);
    invariant(nbits != 0);
    invariant(nbits <= Cache::MaxGetBits);
    fill(nbits);
    return peekBitsNoFill(nbits);
  }

  void skipBits(int nbits) {
    establishClassInvariants();
    fill(nbits);
    skipBitsNoFill(nbits);
  }

  uint32_t getBits(int nbits) {
    establishClassInvariants();
    invariant(nbits >= 0);
    invariant(nbits != 0);
    invariant(nbits <= Cache::MaxGetBits);
    fill(nbits);
    return getBitsNoFill(nbits);
  }

  // This may be used to skip arbitrarily large number of *bits*,
  // not limited by the fill level.
  void skipManyBits(int nbits) {
    establishClassInvariants();
    int remainingBitsToSkip = nbits;
    for (; remainingBitsToSkip >= Cache::MaxGetBits;
         remainingBitsToSkip -= Cache::MaxGetBits) {
      fill(Cache::MaxGetBits);
      skipBitsNoFill(Cache::MaxGetBits);
    }
    if (remainingBitsToSkip > 0) {
      fill(remainingBitsToSkip);
      skipBitsNoFill(remainingBitsToSkip);
    }
  }

  // This may be used to skip arbitrarily large number of *bytes*,
  // not limited by the fill level.
  void skipBytes(int nbytes) {
    establishClassInvariants();
    int nbits = 8 * nbytes;
    skipManyBits(nbits);
  }
};

} // namespace rawspeed
