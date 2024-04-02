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

#include "rawspeedconfig.h"
#include "adt/Bit.h"
#include "codes/AbstractPrefixCodeTranscoder.h"
#include <cstdint>
#include <utility>

namespace rawspeed {

template <typename CodeTag>
class AbstractPrefixCodeEncoder : public AbstractPrefixCodeTranscoder<CodeTag> {
public:
  using Base = AbstractPrefixCodeTranscoder<CodeTag>;

  using Tag = typename Base::Tag;
  using Parent = typename Base::Parent;
  using CodeSymbol = typename Base::CodeSymbol;
  using Traits = typename Base::Traits;

  using Base::Base;

  void setup(bool fullDecode_, bool fixDNGBug16_) {
    Base::setup(fullDecode_, fixDNGBug16_);
  }

  static std::pair<uint32_t, uint8_t>
      RAWSPEED_READNONE reduce(int32_t extendedDiff) {
    if (extendedDiff >= 0) {
      auto diff = static_cast<uint32_t>(extendedDiff);
      return {diff, numActiveBits(diff)};
    }
    --extendedDiff;
    auto diff = static_cast<uint32_t>(extendedDiff);
    int len = numSignificantBits(diff) - 1;
    diff = extractLowBitsSafe(diff, len);
    return {diff, len};
  }
};

} // namespace rawspeed
