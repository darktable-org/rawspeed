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

#include "adt/Invariant.h"                    // for invariant
#include "decoders/RawDecoderException.h"     // for ThrowException, ThrowRDE
#include "decompressors/AbstractPrefixCode.h" // for AbstractPrefixCode
#include "decompressors/PrefixCode.h"         // for PrefixCode
#include <algorithm>                          // for copy, equal, fill, max
#include <cassert>                            // for invariant
#include <cstddef>                            // for size_t
#include <cstdint>                            // for uint8_t, uint32_t, uint16_t
#include <functional>                         // for less, less_equal
#include <iterator>                           // for back_insert_iterator, ba...
#include <numeric>                            // for accumulate
#include <type_traits>                        // for is_integral
#include <vector>                             // for vector, vector<>::const_...

namespace rawspeed {

template <typename CodeTag> class AbstractPrefixCodeDecoder {
public:
  using Tag = CodeTag;
  using Parent = AbstractPrefixCode<CodeTag>;
  using CodeSymbol = typename AbstractPrefixCode<CodeTag>::CodeSymbol;
  using Traits = typename AbstractPrefixCode<CodeTag>::Traits;

  PrefixCode<CodeTag> code;

  explicit AbstractPrefixCodeDecoder(PrefixCode<CodeTag> code_)
      : code(std::move(code_)) {}

  void verifyCodeValuesAsDiffLengths() const {
    for (const auto cValue : code.Base::codeValues) {
      if (cValue <= Traits::MaxDiffLength)
        continue;
      ThrowRDE("Corrupt Huffman code: difference length %u longer than %u",
               cValue, Traits::MaxDiffLength);
    }
    assert(maxCodePlusDiffLength() <= 32U);
  }

protected:
  bool fullDecode = true;
  bool fixDNGBug16 = false;

  [[nodiscard]] inline size_t RAWSPEED_READONLY maxCodeLength() const {
    return code.nCodesPerLength.size() - 1;
  }

  [[nodiscard]] inline size_t RAWSPEED_READONLY __attribute__((pure))
  maxCodePlusDiffLength() const {
    return maxCodeLength() + *(std::max_element(code.Base::codeValues.cbegin(),
                                                code.Base::codeValues.cend()));
  }

  void setup(bool fullDecode_, bool fixDNGBug16_) {
    invariant(!fullDecode_ || Traits::SupportsFullDecode);

    this->fullDecode = fullDecode_;
    this->fixDNGBug16 = fixDNGBug16_;

    if (fullDecode) {
      // If we are in a full-decoding mode, we will be interpreting code values
      // as bit length of the following difference, which incurs hard limit
      // of 16 (since we want to need to read at most 32 bits max for a symbol
      // plus difference). Though we could enforce it per-code instead?
      verifyCodeValuesAsDiffLengths();
    }
  }

public:
  [[nodiscard]] bool isFullDecode() const { return fullDecode; }

  bool operator==(const AbstractPrefixCodeDecoder& other) const {
    return code.symbols == other.code.symbols &&
           code.Base::codeValues == other.codeValues;
  }

  template <typename BIT_STREAM, bool FULL_DECODE>
  inline int processSymbol(BIT_STREAM& bs, CodeSymbol symbol,
                           typename Traits::CodeValueTy codeValue) const {
    invariant(symbol.code_len >= 0 &&
              symbol.code_len <= Traits::MaxCodeLenghtBits);

    // If we were only looking for symbol's code value, then just return it.
    if (!FULL_DECODE)
      return codeValue;

    // Else, treat it as the length of following difference
    // that we need to read and extend.
    int diff_l = codeValue;
    invariant(diff_l >= 0 && diff_l <= 16);

    if (diff_l == 16) {
      if (fixDNGBug16)
        bs.skipBitsNoFill(16);
      return -32768;
    }

    invariant(symbol.code_len + diff_l <= 32);
    return diff_l ? extend(bs.getBitsNoFill(diff_l), diff_l) : 0;
  }

  // Figure F.12 – Extending the sign bit of a decoded value in V
  // WARNING: this is *not* your normal 2's complement sign extension!
  inline static int RAWSPEED_READNONE extend(uint32_t diff, uint32_t len) {
    invariant(len > 0);
    auto ret = static_cast<int32_t>(diff);
    if ((diff & (1 << (len - 1))) == 0)
      ret -= (1 << len) - 1;
    return ret;
  }
};

} // namespace rawspeed
