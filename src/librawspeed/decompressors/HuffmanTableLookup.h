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

#include "common/Common.h"                      // for uint32_t, uint16_t
#include "decoders/RawDecoderException.h"       // for ThrowRDE
#include "decompressors/AbstractHuffmanTable.h" // for AbstractHuffmanTable
#include "io/BitStream.h"                       // for BitStreamTraits
#include <cassert>                              // for assert
#include <memory>                               // for allocator_traits<>::...
#include <vector>                               // for vector

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

class HuffmanTableLookup : public AbstractHuffmanTable {
protected:
  // private fields calculated from codesPerBits and codeValues
  // they are index '1' based, so we can directly lookup the value
  // for code length l without decrementing
  std::vector<uint32_t> maxCodeOL;    // index is length of code
  std::vector<uint16_t> codeOffsetOL; // index is length of code

public:
  std::vector<CodeSymbol> setup(bool fullDecode_, bool fixDNGBug16_) {
    AbstractHuffmanTable::setup(fullDecode_, fixDNGBug16_);

    // Figure C.1: make table of Huffman code length for each symbol
    // Figure C.2: generate the codes themselves
    std::vector<CodeSymbol> symbols = generateCodeSymbols();
    assert(symbols.size() == maxCodesCount());

    // Figure F.15: generate decoding tables
    unsigned int maxCodeLength = nCodesPerLength.size() - 1U;
    codeOffsetOL.resize(maxCodeLength + 1UL, 0xFFFF);
    maxCodeOL.resize(maxCodeLength + 1UL, 0xFFFFFFFF);
    for (unsigned int numCodesSoFar = 0, codeLen = 1; codeLen <= maxCodeLength;
         codeLen++) {
      if (!nCodesPerLength[codeLen])
        continue;
      codeOffsetOL[codeLen] = symbols[numCodesSoFar].code - numCodesSoFar;
      numCodesSoFar += nCodesPerLength[codeLen];
      maxCodeOL[codeLen] = symbols[numCodesSoFar - 1].code;
    }

    return symbols;
  }

  template <typename BIT_STREAM>
  inline int decodeCodeValue(BIT_STREAM& bs) const {
    static_assert(BitStreamTraits<BIT_STREAM>::canUseWithHuffmanTable,
                  "This BitStream specialization is not marked as usable here");
    assert(!fullDecode);
    return decode<BIT_STREAM, false>(bs);
  }

  template <typename BIT_STREAM>
  inline int decodeDifference(BIT_STREAM& bs) const {
    static_assert(BitStreamTraits<BIT_STREAM>::canUseWithHuffmanTable,
                  "This BitStream specialization is not marked as usable here");
    assert(fullDecode);
    return decode<BIT_STREAM, true>(bs);
  }

protected:
  template <typename BIT_STREAM>
  inline std::pair<CodeSymbol, int /*codeValue*/>
  finishReadingPartialSymbol(BIT_STREAM& bs, CodeSymbol partial) const {
    while (partial.code_len < maxCodeOL.size() &&
           (0xFFFFFFFF == maxCodeOL[partial.code_len] ||
            partial.code > maxCodeOL[partial.code_len])) {
      uint32_t temp = bs.getBitsNoFill(1);
      partial.code = (partial.code << 1) | temp;
      partial.code_len++;
    }

    if (partial.code_len >= maxCodeOL.size() ||
        (0xFFFFFFFF == maxCodeOL[partial.code_len] ||
         partial.code > maxCodeOL[partial.code_len]) ||
        partial.code < codeOffsetOL[partial.code_len])
      ThrowRDE("bad Huffman code: %u (len: %u)", partial.code,
               partial.code_len);

    int codeValue = codeValues[partial.code - codeOffsetOL[partial.code_len]];

    return {partial, codeValue};
  }

  template <typename BIT_STREAM>
  inline std::pair<CodeSymbol, int /*codeValue*/>
  readSymbol(BIT_STREAM& bs) const {
    // Start from completely unknown symbol.
    CodeSymbol partial;
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
  inline int decode(BIT_STREAM& bs) const {
    static_assert(BitStreamTraits<BIT_STREAM>::canUseWithHuffmanTable,
                  "This BitStream specialization is not marked as usable here");
    assert(FULL_DECODE == fullDecode);
    bs.fill(32);

    CodeSymbol symbol;
    int codeValue;
    std::tie(symbol, codeValue) = readSymbol(bs);

    return processSymbol<BIT_STREAM, FULL_DECODE>(bs, symbol, codeValue);
  }
};

} // namespace rawspeed
