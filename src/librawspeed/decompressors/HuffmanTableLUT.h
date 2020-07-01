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

#include "common/Common.h"                      // for extractHighBits
#include "decoders/RawDecoderException.h"       // for ThrowRDE
#include "decompressors/AbstractHuffmanTable.h" // for AbstractHuffmanTable...
#include "decompressors/HuffmanTableLookup.h"   // for HuffmanTableLookup
#include "io/BitStream.h"                       // for BitStreamTraits
#include <cassert>                              // for assert
#include <cstddef>                              // for size_t
#include <cstdint>                              // for int32_t, uint16_t
#include <memory>                               // for allocator_traits<>::...
#include <tuple>                                // for tie
#include <vector>                               // for vector
// IWYU pragma: no_include <algorithm>

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

class HuffmanTableLUT final : public HuffmanTableLookup {
  // The code can be compiled with two different decode lookup table layouts.
  // The idea is that different CPU architectures may perform better with
  // one or the other, depending on the relative performance of their arithmetic
  // core vs their memory access. For an Intel Core i7, the big table is better.
#if 1
  // lookup table containing 3 fields: payload:16|flag:8|len:8
  // The payload may be the fully decoded diff or the length of the diff.
  // The len field contains the number of bits, this lookup consumed.
  // A lookup value of 0 means the code was too big to fit into the table.
  // The optimal LookupDepth is also likely to depend on the CPU architecture.
  static constexpr unsigned PayloadShift = 16;
  static constexpr unsigned FlagMask = 0x100;
  static constexpr unsigned LenMask = 0xff;
  static constexpr unsigned LookupDepth = 11;
  std::vector<int32_t> decodeLookup;
#else
  // lookup table containing 2 fields: payload:4|len:4
  // the payload is the length of the diff, len is the length of the code
  static constexpr unsigned LookupDepth = 15;
  static constexpr unsigned PayloadShift = 4;
  static constexpr unsigned FlagMask = 0;
  static constexpr unsigned LenMask = 0x0f;
  std::vector<uint8_t> decodeLookup;
#endif

public:
  void setup(bool fullDecode_, bool fixDNGBug16_) {
    const std::vector<CodeSymbol> symbols =
        HuffmanTableLookup::setup(fullDecode_, fixDNGBug16_);

    // Generate lookup table for fast decoding lookup.
    // See definition of decodeLookup above
    decodeLookup.resize(1 << LookupDepth);
    for (size_t i = 0; i < symbols.size(); i++) {
      uint8_t code_l = symbols[i].code_len;
      if (code_l > static_cast<int>(LookupDepth))
        break;

      uint16_t ll = symbols[i].code << (LookupDepth - code_l);
      uint16_t ul = ll | ((1 << (LookupDepth - code_l)) - 1);
      uint16_t diff_l = codeValues[i];
      for (uint16_t c = ll; c <= ul; c++) {
        if (!(c < decodeLookup.size()))
          ThrowRDE("Corrupt Huffman");

        if (!FlagMask || !fullDecode || code_l > LookupDepth ||
            (code_l + diff_l > LookupDepth && diff_l != 16)) {
          // lookup bit depth is too small to fit both the encoded length
          // and the final difference value.
          // -> store only the length and do a normal sign extension later
          assert(!fullDecode || diff_l > 0);
          decodeLookup[c] = diff_l << PayloadShift | code_l;

          if (!fullDecode)
            decodeLookup[c] |= FlagMask;
        } else {
          // Lookup bit depth is sufficient to encode the final value.
          decodeLookup[c] = FlagMask | code_l;
          if (diff_l != 16 || fixDNGBug16)
            decodeLookup[c] += diff_l;

          if (diff_l) {
            uint32_t diff;
            if (diff_l != 16) {
              diff = extractHighBits(c, code_l + diff_l,
                                     /*effectiveBitwidth=*/LookupDepth);
              diff &= ((1 << diff_l) - 1);
            } else
              diff = -32768;
            decodeLookup[c] |= static_cast<int32_t>(
                static_cast<uint32_t>(extend(diff, diff_l)) << PayloadShift);
          }
        }
      }
    }
  }

  template <typename BIT_STREAM>
  inline __attribute__((always_inline)) int
  decodeCodeValue(BIT_STREAM& bs) const {
    static_assert(BitStreamTraits<BIT_STREAM>::canUseWithHuffmanTable,
                  "This BitStream specialization is not marked as usable here");
    assert(!fullDecode);
    return decode<BIT_STREAM, false>(bs);
  }

  template <typename BIT_STREAM>
  inline __attribute__((always_inline)) int
  decodeDifference(BIT_STREAM& bs) const {
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
  inline __attribute__((always_inline)) int decode(BIT_STREAM& bs) const {
    static_assert(BitStreamTraits<BIT_STREAM>::canUseWithHuffmanTable,
                  "This BitStream specialization is not marked as usable here");
    assert(FULL_DECODE == fullDecode);
    bs.fill(32);

    CodeSymbol partial;
    partial.code_len = LookupDepth;
    partial.code = bs.peekBitsNoFill(partial.code_len);

    assert(partial.code < decodeLookup.size());
    auto lutEntry = static_cast<unsigned>(decodeLookup[partial.code]);
    int payload = static_cast<int>(lutEntry) >> PayloadShift;
    int len = lutEntry & LenMask;

    // How far did reading of those LookupDepth bits *actually* move us forward?
    bs.skipBitsNoFill(len);

    // If the flag bit is set, then we have already skipped all the len bits
    // we needed to skip, and payload is the answer we were looking for.
    if (lutEntry & FlagMask)
      return payload;

    int codeValue;
    if (lutEntry) {
      // If the flag is not set, but the entry is not empty,
      // the payload is the code value for this symbol.
      partial.code_len = len;
      codeValue = payload;
      assert(!FULL_DECODE || codeValue /*aka diff_l*/ > 0);
    } else {
      // No match in the lookup table, because either the code is longer
      // than LookupDepth or the input is corrupt. Need to read more bits...
      assert(len == 0);
      bs.skipBitsNoFill(partial.code_len);
      std::tie(partial, codeValue) = finishReadingPartialSymbol(bs, partial);
    }

    return processSymbol<BIT_STREAM, FULL_DECODE>(bs, partial, codeValue);
  }
};

} // namespace rawspeed
