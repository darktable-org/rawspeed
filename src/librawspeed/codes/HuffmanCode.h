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
#include "adt/Array1DRef.h"
#include "adt/Invariant.h"
#include "codes/AbstractPrefixCode.h"
#include "codes/PrefixCode.h"
#include "decoders/RawDecoderException.h"
#include "io/Buffer.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <numeric>
#include <vector>

namespace rawspeed {

template <typename CodeTag>
class HuffmanCode final : public AbstractPrefixCode<CodeTag> {
public:
  using Parent = AbstractPrefixCode<CodeTag>;
  using CodeSymbol = typename AbstractPrefixCode<CodeTag>::CodeSymbol;
  using Traits = typename AbstractPrefixCode<CodeTag>::Traits;

  HuffmanCode() = default;

protected:
  [[nodiscard]] size_t RAWSPEED_READONLY maxCodeLength() const {
    return nCodesPerLength.size() - 1;
  }

  // These two fields directly represent the contents of a JPEG DHT field

  // 1. The number of codes there are per bit length, this is index 1 based.
  // (there are always 0 codes of length 0)
  std::vector<unsigned int> nCodesPerLength; // index is length of code

  [[nodiscard]] unsigned int RAWSPEED_READONLY maxCodesCount() const {
    return std::accumulate(nCodesPerLength.begin(), nCodesPerLength.end(), 0U);
  }

public:
  [[nodiscard]] std::vector<CodeSymbol> generateCodeSymbols() const {
    std::vector<CodeSymbol> symbols;

    assert(!nCodesPerLength.empty());
    assert(maxCodesCount() > 0);

    assert(this->codeValues.size() == maxCodesCount());

    // reserve all the memory. avoids lots of small allocs
    symbols.reserve(maxCodesCount());

    // Figure C.1: make table of Huffman code length for each symbol
    // Figure C.2: generate the codes themselves
    uint32_t code = 0;
    for (unsigned int l = 1; l <= maxCodeLength(); ++l) {
      for (unsigned int i = 0; i < nCodesPerLength[l]; ++i) {
        symbols.emplace_back(code, l);
        code++;
      }

      code <<= 1;
    }

    assert(symbols.size() == maxCodesCount());

    return symbols;
  }

  bool operator==(const HuffmanCode& other) const {
    return nCodesPerLength == other.nCodesPerLength &&
           this->codeValues == other.codeValues;
  }

  uint32_t setNCodesPerLength(Buffer data) {
    invariant(data.getSize() == Traits::MaxCodeLenghtBits);

    nCodesPerLength.resize((1 + Traits::MaxCodeLenghtBits), 0);
    std::copy(data.begin(), data.end(), &nCodesPerLength[1]);
    assert(nCodesPerLength[0] == 0);

    // trim empty entries from the codes per length table on the right
    while (!nCodesPerLength.empty() && nCodesPerLength.back() == 0)
      nCodesPerLength.pop_back();

    if (nCodesPerLength.empty())
      ThrowRDE("Codes-per-length table is empty");

    assert(nCodesPerLength.back() > 0);

    const auto count = maxCodesCount();
    invariant(count > 0);

    if (count > Traits::MaxNumCodeValues)
      ThrowRDE("Too big code-values table");

    // We are at the Root node, len is 1, there are two possible child Nodes
    unsigned maxCodes = 2;

    for (auto codeLen = 1UL; codeLen < nCodesPerLength.size(); codeLen++) {
      // we have codeLen bits. make sure that that code count can actually fit
      // E.g. for len 1 we could have two codes: 0b0 and 0b1
      // (but in that case there can be no other codes (with higher lengths))
      const auto maxCodesInCurrLen = (1U << codeLen);
      const auto nCodes = nCodesPerLength[codeLen];
      if (nCodes > maxCodesInCurrLen) {
        ThrowRDE("Corrupt Huffman. Can never have %u codes in %lu-bit len",
                 nCodes, codeLen);
      }

      // Also, check that we actually can have this much leafs for this length
      if (nCodes > maxCodes) {
        ThrowRDE(
            "Corrupt Huffman. Can only fit %u out of %u codes in %lu-bit len",
            maxCodes, nCodes, codeLen);
      }

      // There are nCodes leafs on this level, and those can not be branches
      maxCodes -= nCodes;
      // On the next level, rest can be branches, and can have two child Nodes
      maxCodes *= 2;
    }

    return count;
  }

  void setCodeValues(Array1DRef<const typename Traits::CodeValueTy> data) {
    invariant(data.size() <= Traits::MaxNumCodeValues);
    invariant(static_cast<unsigned>(data.size()) == maxCodesCount());

    this->codeValues.clear();
    this->codeValues.reserve(maxCodesCount());
    std::copy(data.begin(), data.end(), std::back_inserter(this->codeValues));
    assert(this->codeValues.size() == maxCodesCount());

    for (const auto& cValue : this->codeValues) {
      if (cValue <= Traits::MaxCodeValue)
        continue;
      ThrowRDE("Corrupt Huffman code: code value %u is larger than maximum %u",
               cValue, Traits::MaxCodeValue);
    }
  }

  explicit operator PrefixCode<CodeTag>() {
    std::vector<CodeSymbol> symbols = generateCodeSymbols();
    return {std::move(symbols), std::move(Parent::codeValues)};
  }
};

} // namespace rawspeed
