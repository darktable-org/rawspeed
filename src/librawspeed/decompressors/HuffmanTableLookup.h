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

#include "common/Common.h"                      // for ushort16, uchar8, int32
#include "decoders/RawDecoderException.h"       // for ThrowRDE
#include "decompressors/AbstractHuffmanTable.h" // for AbstractHuffmanTable
#include "io/BitStream.h"                       // for BitStreamTraits
#include "io/Buffer.h"                          // for Buffer
#include <algorithm>                            // for copy
#include <cassert>                              // for assert
#include <cstddef>                              // for size_t
#include <iterator>                             // for distance
#include <numeric>                              // for accumulate
#include <vector>                               // for vector, allocator, ...

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

class HuffmanTableLookup final : public AbstractHuffmanTable {
  // private fields calculated from codesPerBits and codeValues
  // they are index '1' based, so we can directly lookup the value
  // for code length l without decrementing
  std::vector<ushort16> maxCodeOL;    // index is length of code
  std::vector<ushort16> codeOffsetOL; // index is length of code

  bool fullDecode = true;
  bool fixDNGBug16 = false;

public:
  void setup(bool fullDecode_, bool fixDNGBug16_) {
    this->fullDecode = fullDecode_;
    this->fixDNGBug16 = fixDNGBug16_;

    assert(!nCodesPerLength.empty());
    assert(maxCodesCount() > 0);

    unsigned int maxCodeLength = nCodesPerLength.size() - 1U;
    assert(codeValues.size() == maxCodesCount());

    assert(maxCodePlusDiffLength() <= 32U);

    // Figure C.1: make table of Huffman code length for each symbol
    // Figure C.2: generate the codes themselves
    const auto symbols = generateCodeSymbols();
    assert(symbols.size() == maxCodesCount());

    // Figure F.15: generate decoding tables
    codeOffsetOL.resize(maxCodeLength + 1UL, 0xFFFF);
    maxCodeOL.resize(maxCodeLength + 1UL, 0xFFFF);
    int code_index = 0;
    for (unsigned int l = 1U; l <= maxCodeLength; l++) {
      if (nCodesPerLength[l]) {
        codeOffsetOL[l] = symbols[code_index].code - code_index;
        code_index += nCodesPerLength[l];
        maxCodeOL[l] = symbols[code_index - 1].code;
      }
    }
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

    // 32 is the absolute maximum combined length of code + diff
    // assertion  maxCodePlusDiffLength() <= 32U  is already checked in setup()
    bs.fill(32);

    // for processors supporting bmi2 instructions, using
    // maxCodePlusDiffLength() might be benifitial

    uint32 code = 0;
    uint32 code_l = 0;
    while (code_l < maxCodeOL.size() &&
           (0xFFFF == maxCodeOL[code_l] || code > maxCodeOL[code_l])) {
      uint32 temp = bs.getBitsNoFill(1);
      code = (code << 1) | temp;
      code_l++;
    }

    if (code_l >= maxCodeOL.size() ||
        (0xFFFF == maxCodeOL[code_l] || code > maxCodeOL[code_l]))
      ThrowRDE("bad Huffman code: %u (len: %u)", code, code_l);

    if (code < codeOffsetOL[code_l])
      ThrowRDE("likely corrupt Huffman code: %u (len: %u)", code, code_l);

    int diff_l = codeValues[code - codeOffsetOL[code_l]];

    if (!FULL_DECODE)
      return diff_l;

    if (diff_l == 16) {
      if (fixDNGBug16)
        bs.skipBits(16);
      return -32768;
    }

    assert(FULL_DECODE);
    assert((diff_l && (code_l + diff_l <= 32)) || !diff_l);
    return diff_l ? signExtended(bs.getBitsNoFill(diff_l), diff_l) : 0;
  }
};

} // namespace rawspeed
