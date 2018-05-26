/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2017 Axel Waggershauser
    Copyright (C) 2018 Roman Lebedev

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

#include "decoders/RawDecoderException.h"       // for ThrowRDE
#include "decompressors/AbstractHuffmanTable.h" // for AbstractHuffmanTable...
#include "io/BitStream.h"                       // for BitStreamTraits
#include <cassert>                              // for assert
#include <utility>                              // for make_pair, pair
#include <vector>                               // for vector

namespace rawspeed {

class HuffmanTableVector final : public AbstractHuffmanTable {
  std::vector<CodeSymbol> symbols;

  bool fullDecode = true;
  bool fixDNGBug16 = false;

  // Given this code len, which code id is the minimal?
  std::vector<unsigned int> extrCodeIdForLen; // index is length of code

protected:
  template <typename BIT_STREAM>
  inline std::pair<CodeSymbol, unsigned> getSymbol(BIT_STREAM& bs) const {
    static_assert(BitStreamTraits<BIT_STREAM>::canUseWithHuffmanTable,
                  "This BitStream specialization is not marked as usable here");

    CodeSymbol partial;
    unsigned codeId;

    // Read bits until either find the code or detect the uncorrect code
    for (partial.code = 0, partial.code_len = 1;; ++partial.code_len) {
      assert(partial.code_len <= 16);

      // Read one more bit
      const bool bit = bs.getBits(1);

      partial.code <<= 1;
      partial.code |= bit;

      // Given global ordering and the code length, we know the code id range.
      for (codeId = extrCodeIdForLen[partial.code_len];
           codeId < extrCodeIdForLen[1U + partial.code_len]; codeId++) {
        const CodeSymbol& symbol = symbols[codeId];
        if (symbol == partial) // yay, found?
          return std::make_pair(symbol, codeId);
      }

      // Ok, but does any symbol have this same prefix?
      bool haveCommonPrefix = false;
      for (; codeId < symbols.size(); codeId++) {
        const CodeSymbol& symbol = symbols[codeId];
        haveCommonPrefix |= CodeSymbol::HaveCommonPrefix(symbol, partial);
        if (haveCommonPrefix)
          break;
      }

      // If no symbols have this prefix, then the code is invalid.
      if (!haveCommonPrefix) {
        ThrowRDE("bad Huffman code: %u (len: %u)", partial.code,
                 partial.code_len);
      }
    }

    // We have either returned the found symbol, or thrown on uncorrect symbol.
    __builtin_unreachable();
  }

public:
  void setup(bool fullDecode_, bool fixDNGBug16_) {
    this->fullDecode = fullDecode_;
    this->fixDNGBug16 = fixDNGBug16_;

    assert(!nCodesPerLength.empty());
    assert(maxCodesCount() > 0);
    assert(codeValues.size() == maxCodesCount());

    // Figure C.1: make table of Huffman code length for each symbol
    // Figure C.2: generate the codes themselves
    symbols = generateCodeSymbols();
    assert(symbols.size() == maxCodesCount());

    extrCodeIdForLen.reserve(1U + nCodesPerLength.size());
    extrCodeIdForLen.resize(2); // for len 0 and 1, the min code id is always 0
    for (auto codeLen = 1U; codeLen < nCodesPerLength.size(); codeLen++) {
      auto minCodeId = extrCodeIdForLen.back();
      minCodeId += nCodesPerLength[codeLen];
      extrCodeIdForLen.emplace_back(minCodeId);
    }
    assert(extrCodeIdForLen.size() == 1U + nCodesPerLength.size());
  }

  template <typename BIT_STREAM> inline int decodeLength(BIT_STREAM& bs) const {
    static_assert(BitStreamTraits<BIT_STREAM>::canUseWithHuffmanTable,
                  "This BitStream specialization is not marked as usable here");
    assert(!fullDecode);
    return decode<BIT_STREAM, false>(bs);
  }

  template <typename BIT_STREAM> inline int decodeNext(BIT_STREAM& bs) const {
    static_assert(BitStreamTraits<BIT_STREAM>::canUseWithHuffmanTable,
                  "This BitStream specialization is not marked as usable here");
    assert(fullDecode);
    return decode<BIT_STREAM, true>(bs);
  }

  // The bool template paraeter is to enable two versions:
  // one returning only the length of the of diff bits (see Hasselblad),
  // one to return the fully decoded diff.
  // All ifs depending on this bool will be optimized out by the compiler
  template <typename BIT_STREAM, bool FULL_DECODE>
  inline int decode(BIT_STREAM& bs) const {
    static_assert(BitStreamTraits<BIT_STREAM>::canUseWithHuffmanTable,
                  "This BitStream specialization is not marked as usable here");
    assert(FULL_DECODE == fullDecode);

    const auto got = getSymbol(bs);
    const unsigned codeId = got.second;

    const int diff_l = codeValues[codeId];

    if (!FULL_DECODE)
      return diff_l;

    if (diff_l == 16) {
      if (fixDNGBug16)
        bs.skipBits(16);
      return -32768;
    }

    return diff_l ? signExtended(bs.getBits(diff_l), diff_l) : 0;
  }
};

} // namespace rawspeed
