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
#include "io/Endianness.h"
#include "io/IOException.h"
#include <array>
#include <climits>
#include <cstdint>

namespace rawspeed {

template <typename BIT_STREAM> struct BitStreamerTraits;

template <typename Tag> struct BitStreamerReplenisherBase {
  using size_type = int32_t;

  Array1DRef<const uint8_t> input;
  int pos = 0;

  void establishClassInvariants() const noexcept;

  BitStreamerReplenisherBase() = delete;

  inline explicit BitStreamerReplenisherBase(Array1DRef<const uint8_t> input_)
      : input(input_) {
    if (input.size() < BitStreamerTraits<Tag>::MaxProcessBytes)
      ThrowIOE("Bit stream size is smaller than MaxProcessBytes");
  }

  // A temporary intermediate buffer that may be used by fill() method either
  // in debug build to enforce lack of out-of-bounds reads, or when we are
  // nearing the end of the input buffer and can not just read
  // BitStreamerTraits<Tag>::MaxProcessBytes from it, but have to read as much
  // as we can and fill rest with zeros.
  std::array<uint8_t, BitStreamerTraits<Tag>::MaxProcessBytes> tmpStorage = {};
  inline Array1DRef<uint8_t> tmp() noexcept RAWSPEED_READONLY {
    return {tmpStorage.data(), implicit_cast<int>(tmpStorage.size())};
  }
};

template <typename Tag>
__attribute__((always_inline)) inline void
BitStreamerReplenisherBase<Tag>::establishClassInvariants() const noexcept {
  input.establishClassInvariants();
  invariant(input.size() >= BitStreamerTraits<Tag>::MaxProcessBytes);
  invariant(pos >= 0);
  // `pos` *could* be out-of-bounds of `input`.
}

template <typename Tag>
struct BitStreamerForwardSequentialReplenisher final
    : public BitStreamerReplenisherBase<Tag> {
  using Base = BitStreamerReplenisherBase<Tag>;

  using Base::BitStreamerReplenisherBase;

  BitStreamerForwardSequentialReplenisher() = delete;

  [[nodiscard]] inline typename Base::size_type getPos() const {
    Base::establishClassInvariants();
    return Base::pos;
  }
  [[nodiscard]] inline typename Base::size_type getRemainingSize() const {
    Base::establishClassInvariants();
    return Base::input.size() - getPos();
  }
  inline void markNumBytesAsConsumed(typename Base::size_type numBytes) {
    Base::establishClassInvariants();
    invariant(numBytes >= 0);
    invariant(numBytes != 0);
    Base::pos += numBytes;
  }

  inline Array1DRef<const uint8_t> getInput() {
    Base::establishClassInvariants();

#if !defined(DEBUG)
    // Do we have BitStreamerTraits<Tag>::MaxProcessBytes or more bytes left in
    // the input buffer? If so, then we can just read from said buffer.
    if (getPos() + BitStreamerTraits<Tag>::MaxProcessBytes <=
        Base::input.size()) {
      return Base::input
          .getCrop(getPos(), BitStreamerTraits<Tag>::MaxProcessBytes)
          .getAsArray1DRef();
    }
#endif

    // We have to use intermediate buffer, either because the input is running
    // out of bytes, or because we want to enforce bounds checking.

    // Note that in order to keep all fill-level invariants we must allow to
    // over-read past-the-end a bit.
    if (getPos() >
        Base::input.size() + 2 * BitStreamerTraits<Tag>::MaxProcessBytes)
      ThrowIOE("Buffer overflow read in BitStreamer");

    variableLengthLoadNaiveViaMemcpy(Base::tmp(), Base::input, getPos());

    return Base::tmp();
  }
};

template <typename Derived,
          typename Replenisher =
              BitStreamerForwardSequentialReplenisher<Derived>>
class BitStreamer {
public:
  using size_type = int32_t;
  using Traits = BitStreamerTraits<Derived>;
  using StreamTraits = BitStreamTraits<typename Traits::Stream>;

  using Cache = typename StreamTraits::StreamFlow;

protected:
  Cache cache;

private:
  Replenisher replenisher;

  // this method can be re-implemented in the concrete BitStreamer template
  // specializations. It will return the number of bytes processed. It needs
  // to process up to BitStreamerTraits<Tag>::MaxProcessBytes bytes of input.
  size_type fillCache(Array1DRef<const uint8_t> input) {
    static_assert(BitStreamCacheBase::MaxGetBits >= 32, "check implementation");
    establishClassInvariants();
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

  inline explicit BitStreamer(Array1DRef<const uint8_t> input)
      : replenisher(input) {
    establishClassInvariants();
  }

  inline void fill(int nbits = Cache::MaxGetBits) {
    establishClassInvariants();
    invariant(nbits >= 0);
    invariant(nbits != 0);
    invariant(nbits <= Cache::MaxGetBits);

    if (cache.fillLevel >= nbits)
      return;

    const auto input = replenisher.getInput();
    const auto numBytes = static_cast<Derived*>(this)->fillCache(input);
    replenisher.markNumBytesAsConsumed(numBytes);
  }

  // these methods might be specialized by implementations that support it
  [[nodiscard]] inline size_type RAWSPEED_READONLY getInputPosition() const {
    establishClassInvariants();
    return replenisher.getPos();
  }

  // these methods might be specialized by implementations that support it
  [[nodiscard]] inline size_type getStreamPosition() const {
    establishClassInvariants();
    return getInputPosition() - (cache.fillLevel >> 3);
  }

  [[nodiscard]] inline size_type getRemainingSize() const {
    establishClassInvariants();
    return replenisher.getRemainingSize();
  }

  [[nodiscard]] inline size_type RAWSPEED_READONLY getFillLevel() const {
    establishClassInvariants();
    return cache.fillLevel;
  }

  inline uint32_t RAWSPEED_READONLY peekBitsNoFill(int nbits) {
    establishClassInvariants();
    invariant(nbits >= 0);
    invariant(nbits != 0);
    invariant(nbits <= Cache::MaxGetBits);
    return cache.peek(nbits);
  }

  inline void skipBitsNoFill(int nbits) {
    establishClassInvariants();
    invariant(nbits >= 0);
    // `nbits` could be zero.
    invariant(nbits <= Cache::MaxGetBits);
    cache.skip(nbits);
  }

  inline uint32_t getBitsNoFill(int nbits) {
    establishClassInvariants();
    invariant(nbits >= 0);
    invariant(nbits != 0);
    invariant(nbits <= Cache::MaxGetBits);
    uint32_t ret = peekBitsNoFill(nbits);
    skipBitsNoFill(nbits);
    return ret;
  }

  inline uint32_t peekBits(int nbits) {
    establishClassInvariants();
    invariant(nbits >= 0);
    invariant(nbits != 0);
    invariant(nbits <= Cache::MaxGetBits);
    fill(nbits);
    return peekBitsNoFill(nbits);
  }

  inline void skipBits(int nbits) {
    establishClassInvariants();
    fill(nbits);
    skipBitsNoFill(nbits);
  }

  inline uint32_t getBits(int nbits) {
    establishClassInvariants();
    invariant(nbits >= 0);
    invariant(nbits != 0);
    invariant(nbits <= Cache::MaxGetBits);
    fill(nbits);
    return getBitsNoFill(nbits);
  }

  // This may be used to skip arbitrarily large number of *bytes*,
  // not limited by the fill level.
  inline void skipBytes(int nbytes) {
    establishClassInvariants();
    int remainingBitsToSkip = 8 * nbytes;
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
};

} // namespace rawspeed
