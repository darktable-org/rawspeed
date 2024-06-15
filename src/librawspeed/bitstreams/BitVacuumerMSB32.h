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

#include "bitstreams/BitStreamMSB32.h"
#include "bitstreams/BitVacuumer.h"

namespace rawspeed {

template <typename OutputIterator> class BitVacuumerMSB32;

template <typename OutputIterator>
struct BitVacuumerTraits<BitVacuumerMSB32<OutputIterator>> final {
  static constexpr BitOrder Tag = BitOrder::MSB32;

  static constexpr bool canUseWithPrefixCodeEncoder = true;
};

template <typename OutputIterator>
class BitVacuumerMSB32 final
    : public BitVacuumer<BitVacuumerMSB32<OutputIterator>, OutputIterator> {
  using Base = BitVacuumer<BitVacuumerMSB32<OutputIterator>, OutputIterator>;

public:
  using Base::Base;
};

} // namespace rawspeed
