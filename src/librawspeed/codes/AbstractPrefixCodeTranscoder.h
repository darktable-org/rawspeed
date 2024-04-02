/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2017 Axel Waggershauser
    Copyright (C) 2017-2024 Roman Lebedev

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
#include "codes/AbstractPrefixCode.h"
#include "codes/PrefixCode.h"
#include "decoders/RawDecoderException.h"
#include <algorithm>
#include <cassert>
#include <cstddef>

namespace rawspeed {

template <typename CodeTag> class AbstractPrefixCodeTranscoder {
  bool fullDecode = true;
  bool fixDNGBug16 = false;

public:
  using Tag = CodeTag;
  using Parent = AbstractPrefixCode<CodeTag>;
  using CodeSymbol = typename AbstractPrefixCode<CodeTag>::CodeSymbol;
  using Traits = typename AbstractPrefixCode<CodeTag>::Traits;

  PrefixCode<CodeTag> code;

  explicit AbstractPrefixCodeTranscoder(PrefixCode<CodeTag> code_)
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
  [[nodiscard]] size_t RAWSPEED_READONLY maxCodeLength() const {
    return code.nCodesPerLength.size() - 1;
  }

  [[nodiscard]] size_t RAWSPEED_READONLY __attribute__((pure))
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
  [[nodiscard]] bool RAWSPEED_READONLY isFullDecode() const {
    return fullDecode;
  }
  [[nodiscard]] bool RAWSPEED_READONLY handleDNGBug16() const {
    return fixDNGBug16;
  }

  bool operator==(const AbstractPrefixCodeTranscoder& other) const {
    return code.symbols == other.code.symbols &&
           code.Base::codeValues == other.codeValues;
  }
};

} // namespace rawspeed
