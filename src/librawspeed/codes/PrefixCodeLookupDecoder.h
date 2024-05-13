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

#include "adt/Invariant.h"
#include "bitstreams/BitStreamer.h"
#include "codes/AbstractPrefixCodeDecoder.h"
#include "codes/HuffmanCode.h"
#include "codes/PrefixCode.h"
#include "decoders/RawDecoderException.h"
#include <cassert>
#include <cstdint>
#include <limits>
#include <tuple>
#include <utility>
#include <vector>

/*
 * The following code is inspired by the IJG JPEG library.
 *
 * Copyright (C) 1991, 1992, Thomas G. Lane.
 * Part of the Independent JPEG Group's software.
 * See the file Copyright for more details.
 *
 * Copyright (c) 1993 Brian C. Smith, The Regents of the University
 * of California
 * All rights reserved.
 *
 * Copyright (c) 1994 Kongji Huang and Brian C. Smith.
 * Cornell University
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose, without fee, and without written agreement is
 * hereby granted, provided that the above copyright notice and the following
 * two paragraphs appear in all copies of this software.
 *
 * IN NO EVENT SHALL CORNELL UNIVERSITY BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT
 * OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF CORNELL
 * UNIVERSITY HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * CORNELL UNIVERSITY SPECIFICALLY DISCLAIMS ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND CORNELL UNIVERSITY HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 */

namespace rawspeed {

template <typename CodeTag>
class PrefixCodeLookupDecoder : public AbstractPrefixCodeDecoder<CodeTag> {
public:
  using Tag = CodeTag;
  using Base = AbstractPrefixCodeDecoder<CodeTag>;
  using Traits = typename Base::Traits;

  // We only support true Huffman codes, not generic prefix codes.
  explicit PrefixCodeLookupDecoder(HuffmanCode<CodeTag>&& hc)
      : Base(std::move(hc).operator rawspeed::PrefixCode<CodeTag>()) {}

  PrefixCodeLookupDecoder(PrefixCode<CodeTag>) = delete;
  PrefixCodeLookupDecoder(const PrefixCode<CodeTag>&) = delete;
  PrefixCodeLookupDecoder(PrefixCode<CodeTag>&&) = delete;

protected:
  // private fields calculated from codesPerBits and codeValues
  // they are index '1' based, so we can directly lookup the value
  // for code length l without decrementing
  std::vector<typename Traits::CodeTy> maxCodeOL;    // index is length of code
  std::vector<typename Traits::CodeTy> codeOffsetOL; // index is length of code

  static constexpr auto MaxCodeValue =
      std::numeric_limits<typename Traits::CodeTy>::max();

public:
  void setup(bool fullDecode_, bool fixDNGBug16_) {
    AbstractPrefixCodeDecoder<CodeTag>::setup(fullDecode_, fixDNGBug16_);

    // Figure F.15: generate decoding tables
    codeOffsetOL.resize(Base::maxCodeLength() + 1UL, MaxCodeValue);
    maxCodeOL.resize(Base::maxCodeLength() + 1UL, MaxCodeValue);
    for (unsigned int numCodesSoFar = 0, codeLen = 1;
         codeLen <= Base::maxCodeLength(); codeLen++) {
      if (!Base::code.nCodesPerLength[codeLen])
        continue;
      codeOffsetOL[codeLen] = implicit_cast<typename Traits::CodeTy>(
          Base::code.symbols[numCodesSoFar].code - numCodesSoFar);
      assert(codeOffsetOL[codeLen] != MaxCodeValue);
      numCodesSoFar += Base::code.nCodesPerLength[codeLen];
      maxCodeOL[codeLen] = Base::code.symbols[numCodesSoFar - 1].code;
    }
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

protected:
  template <typename BIT_STREAM>
  std::pair<typename Base::CodeSymbol, int /*codeValue*/>
  finishReadingPartialSymbol(BIT_STREAM& bs,
                             typename Base::CodeSymbol partial) const {
    static_assert(
        BitStreamerTraits<BIT_STREAM>::canUseWithPrefixCodeDecoder,
        "This BitStreamer specialization is not marked as usable here");
    while (partial.code_len < Base::maxCodeLength() &&
           (MaxCodeValue == maxCodeOL[partial.code_len] ||
            partial.code > maxCodeOL[partial.code_len])) {
      uint32_t temp = bs.getBitsNoFill(1);
      partial.code =
          implicit_cast<typename Traits::CodeTy>((partial.code << 1) | temp);
      partial.code_len++;
    }

    // NOTE: when we are called from PrefixCodeLUTDecoder, the partial.code_len
    // *could* be larger than the largest code lenght for this huffman table,
    // which is a symptom of a corrupt code.
    if (partial.code_len > Base::maxCodeLength() ||
        partial.code > maxCodeOL[partial.code_len])
      ThrowRDE("bad Huffman code: %u (len: %u)", partial.code,
               partial.code_len);

    assert(MaxCodeValue != codeOffsetOL[partial.code_len]);
    assert(partial.code >= codeOffsetOL[partial.code_len]);
    unsigned codeIndex = partial.code - codeOffsetOL[partial.code_len];
    assert(codeIndex < Base::code.codeValues.size());

    typename Traits::CodeValueTy codeValue = Base::code.codeValues[codeIndex];
    return {partial, codeValue};
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
  // The bool template paraeter is to enable two versions:
  // one returning only the length of the of diff bits (see Hasselblad),
  // one to return the fully decoded diff.
  // All ifs depending on this bool will be optimized out by the compiler
  template <typename BIT_STREAM, bool FULL_DECODE>
  int decode(BIT_STREAM& bs) const {
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
