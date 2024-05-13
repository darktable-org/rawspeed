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

#include "adt/Invariant.h"
#include "bitstreams/BitStreamer.h"
#include "codes/AbstractPrefixCodeDecoder.h"
#include "decoders/RawDecoderException.h"
#include <cassert>
#include <cstdint>
#include <tuple>
#include <utility>
#include <vector>

namespace rawspeed {

template <typename CodeTag>
class PrefixCodeVectorDecoder : public AbstractPrefixCodeDecoder<CodeTag> {
public:
  using Tag = CodeTag;
  using Base = AbstractPrefixCodeDecoder<CodeTag>;
  using Traits = typename Base::Traits;

  using Base::Base;

private:
  // Given this code len, which code id is the minimal?
  std::vector<uint32_t> extrCodeIdForLen; // index is length of code

protected:
  template <typename BIT_STREAM>
  std::pair<typename Base::CodeSymbol, int /*codeValue*/>
  finishReadingPartialSymbol(BIT_STREAM& bs,
                             typename Base::CodeSymbol partial) const {
    static_assert(
        BitStreamerTraits<BIT_STREAM>::canUseWithPrefixCodeDecoder,
        "This BitStreamer specialization is not marked as usable here");

    // Read bits until either find the code or detect the incorrect code
    while (partial.code_len < Base::maxCodeLength()) {
      // Read one more bit
      const bool bit = bs.getBitsNoFill(1);

      partial.code <<= 1;
      partial.code |= bit;
      partial.code_len++;

      // Given global ordering and the code length, we know the code id range.
      for (auto codeId = extrCodeIdForLen[partial.code_len];
           codeId < extrCodeIdForLen[1U + partial.code_len]; codeId++) {
        const typename Base::CodeSymbol& symbol = Base::code.symbols[codeId];
        invariant(partial.code_len == symbol.code_len);
        if (symbol == partial) // yay, found?
          return {symbol, Base::code.codeValues[codeId]};
      }
    }

    ThrowRDE("bad Huffman code: %u (len: %u)", partial.code, partial.code_len);
  }

  template <typename BIT_STREAM>
  std::pair<typename Base::CodeSymbol, int /*codeValue*/>
  readSymbol(BIT_STREAM& bs) const {
    // Start from completely unknown symbol.
    typename Base::CodeSymbol partial;
    partial.code_len = 0;
    partial.code = 0;

    return finishReadingPartialSymbol(bs, partial);
  }

public:
  void setup(bool fullDecode_, bool fixDNGBug16_) {
    AbstractPrefixCodeDecoder<CodeTag>::setup(fullDecode_, fixDNGBug16_);

    extrCodeIdForLen.reserve(1U + Base::code.nCodesPerLength.size());
    extrCodeIdForLen.resize(2); // for len 0 and 1, the min code id is always 0
    for (auto codeLen = 1UL; codeLen < Base::code.nCodesPerLength.size();
         codeLen++) {
      auto minCodeId = extrCodeIdForLen.back();
      minCodeId += Base::code.nCodesPerLength[codeLen];
      extrCodeIdForLen.emplace_back(minCodeId);
    }
    assert(extrCodeIdForLen.size() == 1U + Base::code.nCodesPerLength.size());
  }

  template <typename BIT_STREAM>
  typename Traits::CodeValueTy decodeCodeValue(BIT_STREAM& bs) const {
    static_assert(
        BitStreamerTraits<BIT_STREAM>::canUseWithPrefixCodeDecoder,
        "This BitStreamer specialization is not marked as usable here");
    invariant(!Base::isFullDecode());
    return decode<BIT_STREAM, false>(bs);
  }

  template <typename BIT_STREAM> int decodeDifference(BIT_STREAM& bs) const {
    static_assert(
        BitStreamerTraits<BIT_STREAM>::canUseWithPrefixCodeDecoder,
        "This BitStreamer specialization is not marked as usable here");
    invariant(Base::isFullDecode());
    return decode<BIT_STREAM, true>(bs);
  }

  // The bool template paraeter is to enable two versions:
  // one returning only the length of the of diff bits (see Hasselblad),
  // one to return the fully decoded diff.
  // All ifs depending on this bool will be optimized out by the compiler
  template <typename BIT_STREAM, bool FULL_DECODE>
  int decode(BIT_STREAM& bs) const {
    static_assert(
        BitStreamerTraits<BIT_STREAM>::canUseWithPrefixCodeDecoder,
        "This BitStreamer specialization is not marked as usable here");
    invariant(FULL_DECODE == Base::isFullDecode());

    bs.fill(32);

    typename Base::CodeSymbol symbol;
    typename Traits::CodeValueTy codeValue;
    std::tie(symbol, codeValue) = readSymbol(bs);

    return Base::template processSymbol<BIT_STREAM, FULL_DECODE>(bs, symbol,
                                                                 codeValue);
  }
};

} // namespace rawspeed
