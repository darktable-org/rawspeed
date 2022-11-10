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

#include "common/Common.h"                // for extractHighBits
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "io/Buffer.h"                    // for Buffer
#include <algorithm>                      // for equal, copy, fill, max
#include <cassert>                        // for assert
#include <cstddef>                        // for size_t
#include <cstdint>                        // for uint8_t, uint32_t, uint16_t
#include <functional>                     // for less, less_equal
#include <iterator>                       // for back_insert_iterator, back...
#include <numeric>                        // for accumulate
#include <vector>                         // for vector, operator==

namespace rawspeed {

class AbstractHuffmanTable {
public:
  struct CodeSymbol final {
    uint16_t code;    // the code (bit pattern found inside the stream)
    uint8_t code_len; // the code length in bits, valid values are 1..16

    CodeSymbol() = default;

    CodeSymbol(uint16_t code_, uint8_t code_len_)
        : code(code_), code_len(code_len_) {
      assert(code_len > 0);
      assert(code_len <= 16);
      assert(code <= ((1U << code_len) - 1U));
    }

    static bool HaveCommonPrefix(const CodeSymbol& symbol,
                                 const CodeSymbol& partial) {
      assert(partial.code_len <= symbol.code_len);

      const auto s0 = extractHighBits(symbol.code, partial.code_len,
                                      /*effectiveBitwidth=*/symbol.code_len);
      const auto s1 = partial.code;

      return s0 == s1;
    }
  };

  void verifyCodeSymbolsAreValidDiffLenghts() const {
    for (const auto cValue : codeValues) {
      if (cValue <= 16)
        continue;
      ThrowRDE("Corrupt Huffman code: difference length %u longer than 16",
               cValue);
    }
    assert(maxCodePlusDiffLength() <= 32U);
  }

protected:
  bool fullDecode = true;
  bool fixDNGBug16 = false;

  [[nodiscard]] inline size_t __attribute__((pure))
  maxCodePlusDiffLength() const {
    return nCodesPerLength.size() - 1 +
           *(std::max_element(codeValues.cbegin(), codeValues.cend()));
  }

  // These two fields directly represent the contents of a JPEG DHT field

  // 1. The number of codes there are per bit length, this is index 1 based.
  // (there are always 0 codes of length 0)
  std::vector<unsigned int> nCodesPerLength; // index is length of code

  [[nodiscard]] inline unsigned int __attribute__((pure))
  maxCodesCount() const {
    return std::accumulate(nCodesPerLength.begin(), nCodesPerLength.end(), 0U);
  }

  // 2. This is the actual huffman encoded data, i.e. the 'alphabet'. Each value
  // is the number of bits following the code that encode the difference to the
  // last pixel. Valid values are in the range 0..16.
  // extend() is used to decode the difference bits to a signed int.
  std::vector<uint8_t> codeValues; // index is just sequential number

  void setup(bool fullDecode_, bool fixDNGBug16_) {
    this->fullDecode = fullDecode_;
    this->fixDNGBug16 = fixDNGBug16_;

    if (fullDecode) {
      // If we are in a full-decoding mode, we will be interpreting code values
      // as bit length of the following difference, which incurs hard limit
      // of 16 (since we want to need to read at most 32 bits max for a symbol
      // plus difference). Though we could enforce it per-code instead?
      verifyCodeSymbolsAreValidDiffLenghts();
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

    const auto maxCodeLength = nCodesPerLength.size() - 1U;
    assert(codeValues.size() == maxCodesCount());

    // reserve all the memory. avoids lots of small allocs
    symbols.reserve(maxCodesCount());

    // Figure C.1: make table of Huffman code length for each symbol
    // Figure C.2: generate the codes themselves
    uint32_t code = 0;
    for (unsigned int l = 1; l <= maxCodeLength; ++l) {
      for (unsigned int i = 0; i < nCodesPerLength[l]; ++i) {
        assert(code <= 0xffff);

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
           codeValues == other.codeValues;
  }

  uint32_t setNCodesPerLength(const Buffer& data) {
    assert(data.getSize() == 16);

    nCodesPerLength.resize(17, 0);
    std::copy(data.begin(), data.end(), &nCodesPerLength[1]);
    assert(nCodesPerLength[0] == 0);

    // trim empty entries from the codes per length table on the right
    while (!nCodesPerLength.empty() && nCodesPerLength.back() == 0)
      nCodesPerLength.pop_back();

    if (nCodesPerLength.empty())
      ThrowRDE("Codes-per-length table is empty");

    assert(nCodesPerLength.back() > 0);

    const auto count = maxCodesCount();
    assert(count > 0);

    if (count > 162)
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

  void setCodeValues(const Buffer& data) {
    // spec says max 16 but Hasselblad ignores that -> allow 17
    // Canon's old CRW really ignores this ...
    assert(data.getSize() <= 162);
    assert(data.getSize() == maxCodesCount());

    codeValues.clear();
    codeValues.reserve(maxCodesCount());
    std::copy(data.begin(), data.end(), std::back_inserter(codeValues));
    assert(codeValues.size() == maxCodesCount());
  }

  template <typename BIT_STREAM, bool FULL_DECODE>
  inline int processSymbol(BIT_STREAM& bs, CodeSymbol symbol,
                           int codeValue) const {
    assert(symbol.code_len >= 0 && symbol.code_len <= 16);

    // If we were only looking for symbol's code value, then just return it.
    if (!FULL_DECODE)
      return codeValue;

    // Else, treat it as the length of following difference
    // that we need to read and extend.
    int diff_l = codeValue;
    assert(diff_l >= 0 && diff_l <= 16);

    if (diff_l == 16) {
      if (fixDNGBug16)
        bs.skipBitsNoFill(16);
      return -32768;
    }

    assert(symbol.code_len + diff_l <= 32);
    return diff_l ? extend(bs.getBitsNoFill(diff_l), diff_l) : 0;
  }

  // Figure F.12 – Extending the sign bit of a decoded value in V
  // WARNING: this is *not* your normal 2's complement sign extension!
  inline static int __attribute__((const)) extend(uint32_t diff, uint32_t len) {
    assert(len > 0);
    auto ret = static_cast<int32_t>(diff);
    if ((diff & (1 << (len - 1))) == 0)
      ret -= (1 << len) - 1;
    return ret;
  }
};

inline bool operator==(const AbstractHuffmanTable::CodeSymbol& lhs,
                       const AbstractHuffmanTable::CodeSymbol& rhs) {
  return lhs.code == rhs.code && lhs.code_len == rhs.code_len;
}

} // namespace rawspeed
