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

#include "adt/Array1DRef.h"                   // for Array1DRef
#include "adt/Invariant.h"                    // for invariant
#include "decoders/RawDecoderException.h"     // for ThrowException, ThrowRDE
#include "decompressors/AbstractPrefixCode.h" // for AbstractPrefixCode
#include "io/Buffer.h"                        // for Buffer
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

template <typename CodeTag>
class AbstractHuffmanTable : public AbstractPrefixCode<CodeTag> {
public:
  using Parent = AbstractPrefixCode<CodeTag>;
  using CodeSymbol = typename AbstractPrefixCode<CodeTag>::CodeSymbol;
  using Traits = typename AbstractPrefixCode<CodeTag>::Traits;

  void verifyCodeValuesAsDiffLengths() const {
    for (const auto cValue : this->codeValues) {
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
    return nCodesPerLength.size() - 1;
  }

  [[nodiscard]] inline size_t RAWSPEED_READONLY __attribute__((pure))
  maxCodePlusDiffLength() const {
    return maxCodeLength() + *(std::max_element(this->codeValues.cbegin(),
                                                this->codeValues.cend()));
  }

  // These two fields directly represent the contents of a JPEG DHT field

  // 1. The number of codes there are per bit length, this is index 1 based.
  // (there are always 0 codes of length 0)
  std::vector<unsigned int> nCodesPerLength; // index is length of code

  [[nodiscard]] inline unsigned int RAWSPEED_READONLY maxCodesCount() const {
    return std::accumulate(nCodesPerLength.begin(), nCodesPerLength.end(), 0U);
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

  static void VerifyCodeSymbols(const std::vector<CodeSymbol>& symbols) {
#ifndef NDEBUG
    // The code symbols are ordered so that all the code values are strictly
    // increasing and code lengths are not decreasing.
    const auto symbolSort = [](const CodeSymbol& lhs, const CodeSymbol& rhs) {
      return std::less<>()(lhs.code, rhs.code) &&
             std::less_equal<>()(lhs.code_len, rhs.code_len);
    };
#endif
    assert(std::adjacent_find(symbols.cbegin(), symbols.cend(),
                              [&symbolSort](const CodeSymbol& lhs,
                                            const CodeSymbol& rhs) -> bool {
                                return !symbolSort(lhs, rhs);
                              }) == symbols.cend() &&
           "all code symbols are globally ordered");

    // No two symbols should have the same prefix (high bytes)
    // Only analyze the lower triangular matrix, excluding diagonal
    for (auto sId = 0UL; sId < symbols.size(); sId++) {
      for (auto pId = 0UL; pId < sId; pId++)
        assert(!CodeSymbol::HaveCommonPrefix(symbols[sId], symbols[pId]));
    }
  }

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
    VerifyCodeSymbols(symbols);

    return symbols;
  }

public:
  [[nodiscard]] bool isFullDecode() const { return fullDecode; }

  bool operator==(const AbstractHuffmanTable& other) const {
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
    invariant((unsigned)data.size() == maxCodesCount());

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
