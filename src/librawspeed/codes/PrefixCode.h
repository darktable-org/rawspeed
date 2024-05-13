/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2017-2023 Roman Lebedev

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

#include "codes/AbstractPrefixCode.h"
#include "decoders/RawDecoderException.h"
#include <algorithm>
#include <cassert>
#include <functional>
#include <vector>

namespace rawspeed {

template <typename CodeTag>
class PrefixCode final : public AbstractPrefixCode<CodeTag> {
public:
  using Base = AbstractPrefixCode<CodeTag>;
  using Traits = typename Base::Traits;
  using CodeSymbol = typename Base::CodeSymbol;
  using CodeValueTy = typename Traits::CodeValueTy;

  // 1. The number of codes there are per bit length, this is index 1 based.
  // (there are always 0 codes of length 0)
  //
  // WARNING: just because two PrefixCode's have matching nCodesPerLength,
  //          does not mean their actual code symbols match!
  std::vector<unsigned> nCodesPerLength;

  // The codes themselves.
  std::vector<CodeSymbol> symbols;

  PrefixCode(std::vector<CodeSymbol> symbols_,
             std::vector<CodeValueTy> codeValues_)
      : Base(std::move(codeValues_)), symbols(std::move(symbols_)) {
    if (symbols.empty() || Base::codeValues.empty() ||
        symbols.size() != Base::codeValues.size())
      ThrowRDE("Malformed code");

    nCodesPerLength.resize(1 + Traits::MaxCodeLenghtBits);
    for (const CodeSymbol& s : symbols) {
      assert(s.code_len > 0 && s.code_len <= Traits::MaxCodeLenghtBits);
      ++nCodesPerLength[s.code_len];
    }
    while (nCodesPerLength.back() == 0)
      nCodesPerLength.pop_back();
    assert(nCodesPerLength.size() > 1);

    verifyCodeSymbols();
  }

private:
  void verifyCodeSymbols() {
    // We are at the Root node, len is 1, there are two possible child Nodes
    unsigned maxCodes = 2;
    for (auto codeLen = 1UL; codeLen < nCodesPerLength.size(); codeLen++) {
      // we have codeLen bits. make sure that that code count can actually fit
      // E.g. for len 1 we could have two codes: 0b0 and 0b1
      // (but in that case there can be no other codes (with higher lengths))
      const unsigned nCodes = nCodesPerLength[codeLen];
      if (nCodes > maxCodes)
        ThrowRDE("Too many codes of of length %lu.", codeLen);
      // There are nCodes leafs on this level, and those can not be branches
      maxCodes -= nCodes;
      // On the next level, rest can be branches, and can have two child Nodes
      maxCodes *= 2;
    }

    // The code symbols are ordered so that the code lengths are not decreasing.
    // NOTE: codes of same lenght are not nessesairly sorted!
    if (std::adjacent_find(
            symbols.cbegin(), symbols.cend(),
            [](const CodeSymbol& lhs, const CodeSymbol& rhs) -> bool {
              return !std::less_equal<>()(lhs.code_len, rhs.code_len);
            }) != symbols.cend())
      ThrowRDE("Code symbols are not globally ordered");

    // No two symbols should have the same prefix (high bits)
    // Only analyze the lower triangular matrix, excluding diagonal
    for (auto sId = 0UL; sId < symbols.size(); sId++) {
      for (auto pId = 0UL; pId < sId; pId++)
        if (CodeSymbol::HaveCommonPrefix(symbols[sId], symbols[pId]))
          ThrowRDE("Not prefix codes!");
    }
  }
};

} // namespace rawspeed
