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
#include "bitstreams/BitStream.h"
#include "bitstreams/BitStreamMSB.h"
#include "bitstreams/BitVacuumer.h"
#include "io/Endianness.h"
#include <cstddef>
#include <cstdint>

namespace rawspeed {

template <typename OutputIterator> class BitVacuumerMSB;

template <typename OutputIterator>
struct BitVacuumerTraits<BitVacuumerMSB<OutputIterator>> final {
  using Stream = BitStreamMSB;
};

template <typename OutputIterator>
class BitVacuumerMSB final
    : public BitVacuumer<BitVacuumerMSB<OutputIterator>, OutputIterator> {
  using Base = BitVacuumer<BitVacuumerMSB<OutputIterator>, OutputIterator>;

  friend void Base::drain(); // Allow it to actually call `drainImpl()`.

  inline void drainImpl() {
    invariant(Base::cache.fillLevel >= Base::chunk_bitwidth);

    typename Base::chunk_type chunk = Base::cache.peek(Base::chunk_bitwidth);
    chunk = getBE<typename Base::chunk_type>(&chunk);
    Base::cache.skip(Base::chunk_bitwidth);

    const auto bytes = Array1DRef<const std::byte>(Array1DRef(&chunk, 1));
    for (const auto byte : bytes)
      *Base::output = static_cast<uint8_t>(byte);
  }

public:
  using Base::Base;
};

} // namespace rawspeed
