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

#include "io/BitStreamer.h"
#include "io/BitVacuumer.h"
#include <cstdint>
#include <iterator>

namespace rawspeed {

struct MSBBitVacuumerTag;

template <typename OutputIterator>
using BitVacuumerMSB =
    BitVacuumer<MSBBitVacuumerTag, BitStreamerCacheRightInLeftOut,
                OutputIterator>;

template <typename Class>
  requires(std::same_as<Class, BitVacuumerMSB<typename Class::OutputIterator>>)
inline void bitVacuumerCacheDrainer(Class& This) {
  invariant(This.cache.fillLevel >= Class::chunk_bitwidth);

  typename Class::chunk_type chunk = This.cache.peek(Class::chunk_bitwidth);
  chunk = getBE<typename Class::chunk_type>(&chunk);
  This.cache.skip(Class::chunk_bitwidth);

  const auto bytes = Array1DRef<const std::byte>(Array1DRef(&chunk, 1));
  for (const auto byte : bytes)
    *This.output = static_cast<uint8_t>(byte);
}

} // namespace rawspeed
