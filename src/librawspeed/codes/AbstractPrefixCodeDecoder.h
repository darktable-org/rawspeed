/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2017 Axel Waggershauser
    Copyright (C) 2017-2018 Roman Lebedev

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
#include "adt/Invariant.h"
#include "codes/AbstractPrefixCodeTranscoder.h"
#include <cstdint>

namespace rawspeed {

template <typename CodeTag>
class AbstractPrefixCodeDecoder : public AbstractPrefixCodeTranscoder<CodeTag> {
public:
  using Base = AbstractPrefixCodeTranscoder<CodeTag>;

  using Tag = typename Base::Tag;
  using Parent = typename Base::Parent;
  using CodeSymbol = typename Base::CodeSymbol;
  using Traits = typename Base::Traits;

  using Base::Base;

  template <typename BIT_STREAM, bool FULL_DECODE>
  int processSymbol(BIT_STREAM& bs, CodeSymbol symbol,
                    typename Traits::CodeValueTy codeValue) const {
    invariant(symbol.code_len >= 0 &&
              symbol.code_len <= Traits::MaxCodeLenghtBits);

    // If we were only looking for symbol's code value, then just return it.
    if constexpr (!FULL_DECODE)
      return codeValue;

    // Else, treat it as the length of following difference
    // that we need to read and extend.
    int diff_l = codeValue;
    invariant(diff_l >= 0 && diff_l <= 16);

    if (diff_l == 16) {
      if (Base::handleDNGBug16())
        bs.skipBitsNoFill(16);
      return -32768;
    }

    invariant(symbol.code_len + diff_l <= 32);
    return diff_l ? extend(bs.getBitsNoFill(diff_l), diff_l) : 0;
  }

  // Figure F.12 – Extending the sign bit of a decoded value in V
  // WARNING: this is *not* your normal 2's complement sign extension!
  static int RAWSPEED_READNONE extend(uint32_t diff, uint32_t len) {
    invariant(len > 0);
    auto ret = static_cast<int32_t>(diff);
    if ((diff & (1 << (len - 1))) == 0)
      ret -= (1 << len) - 1;
    return ret;
  }
};

} // namespace rawspeed
