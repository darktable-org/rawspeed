/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2024 Roman Lebedev

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

#include "adt/Array1DRef.h"
#include "adt/Invariant.h"
#include "bitstreams/BitStreamJPEG.h"
#include "bitstreams/BitVacuumer.h"
#include "io/Endianness.h"
#include <cstddef>
#include <cstdint>

namespace rawspeed {

template <typename OutputIterator> class BitVacuumerJPEG;

template <typename OutputIterator>
struct BitVacuumerTraits<BitVacuumerJPEG<OutputIterator>> final {
  using Stream = BitStreamJPEG;

  static constexpr bool canUseWithPrefixCodeEncoder = true;
};

template <typename OutputIterator>
class BitVacuumerJPEG final
    : public BitVacuumer<BitVacuumerJPEG<OutputIterator>, OutputIterator> {
  using Base = BitVacuumer<BitVacuumerJPEG<OutputIterator>, OutputIterator>;
  using StreamTraits = typename Base::StreamTraits;

  friend void Base::drain(); // Allow it to actually call `drainImpl()`.

  inline void drainImpl() {
    invariant(Base::cache.fillLevel >= Base::chunk_bitwidth);
    invariant(Base::chunk_bitwidth == 32);

    constexpr int StreamChunkBitwidth =
        bitwidth<typename StreamTraits::ChunkType>();
    static_assert(Base::chunk_bitwidth >= StreamChunkBitwidth);
    static_assert(Base::chunk_bitwidth % StreamChunkBitwidth == 0);
    constexpr int NumChunksNeeded = Base::chunk_bitwidth / StreamChunkBitwidth;
    static_assert(NumChunksNeeded == 1);

    auto chunk = implicit_cast<typename StreamTraits::ChunkType>(
        Base::cache.peek(StreamChunkBitwidth));
    chunk = getByteSwapped<typename StreamTraits::ChunkType>(
        &chunk, StreamTraits::ChunkEndianness != getHostEndianness());
    Base::cache.skip(StreamChunkBitwidth);

    const auto bytes = Array1DRef<const std::byte>(Array1DRef(&chunk, 1));
    for (const auto byte : bytes) {
      *Base::output = static_cast<uint8_t>(byte);
      if (static_cast<uint8_t>(byte) == 0xFF)
        *Base::output = uint8_t(0x00); // Stuffing byte
    }
  }

public:
  using Base::Base;
};

} // namespace rawspeed
