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

#include "adt/Bit.h"
#include "adt/Casts.h"
#include "adt/Invariant.h"
#include "bitstreams/BitStreamer.h"
#include "decoders/RawDecoderException.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <tuple>
#include <type_traits>
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

template <typename CodeTag, typename BackendPrefixCodeDecoder>
class PrefixCodeLUTDecoder : public BackendPrefixCodeDecoder {
public:
  using Tag = CodeTag;
  using Base = BackendPrefixCodeDecoder;
  using Traits = typename Base::Traits;

  // NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
  using Base::Base;

private:
  // lookup table containing 3 fields: payload:16|flag:8|len:8
  // The payload may be the fully decoded diff or the length of the diff.
  // The len field contains the number of bits, this lookup consumed.
  // A lookup value of 0 means the code was too big to fit into the table.
  // The optimal LookupDepth is also likely to depend on the CPU architecture.
  static constexpr unsigned PayloadShift = 9;
  static constexpr unsigned FlagMask = 0x100;
  static constexpr unsigned LenMask = 0xff;
  static constexpr unsigned LookupDepth = 11;
  using LUTEntryTy = int32_t;
  using LUTUnsignedEntryTy = std::make_unsigned_t<LUTEntryTy>;
  std::vector<LUTEntryTy> decodeLookup;

public:
  void setup(bool fullDecode_, bool fixDNGBug16_) {
    Base::setup(fullDecode_, fixDNGBug16_);

    // Generate lookup table for fast decoding lookup.
    // See definition of decodeLookup above
    decodeLookup.resize(1 << LookupDepth);
    for (size_t i = 0; i < Base::code.symbols.size(); i++) {
      uint8_t code_l = Base::code.symbols[i].code_len;
      if (code_l > static_cast<int>(LookupDepth))
        break;

      auto ll = implicit_cast<uint16_t>(Base::code.symbols[i].code
                                        << (LookupDepth - code_l));
      auto ul =
          implicit_cast<uint16_t>(ll | ((1 << (LookupDepth - code_l)) - 1));
      static_assert(Traits::MaxCodeValueLenghtBits <=
                    bitwidth<LUTEntryTy>() - PayloadShift);
      LUTUnsignedEntryTy diff_l = Base::code.codeValues[i];
      for (uint16_t c = ll; c <= ul; c++) {
        if (!(c < decodeLookup.size()))
          ThrowRDE("Corrupt Huffman");

        if (!FlagMask || !Base::isFullDecode() || code_l > LookupDepth ||
            (code_l + diff_l > LookupDepth && diff_l != 16)) {
          // lookup bit depth is too small to fit both the encoded length
          // and the final difference value.
          // -> store only the length and do a normal sign extension later
          invariant(!Base::isFullDecode() || diff_l > 0);
          decodeLookup[c] = diff_l << PayloadShift | code_l;

          if (!Base::isFullDecode())
            decodeLookup[c] |= FlagMask;
        } else {
          // Lookup bit depth is sufficient to encode the final value.
          decodeLookup[c] = FlagMask | code_l;
          if (diff_l != 16 || Base::handleDNGBug16())
            decodeLookup[c] += diff_l;

          if (diff_l) {
            LUTUnsignedEntryTy diff;
            if (diff_l != 16) {
              diff = extractHighBits(c, code_l + diff_l,
                                     /*effectiveBitwidth=*/LookupDepth);
              diff &= ((1 << diff_l) - 1);
            } else
              diff = LUTUnsignedEntryTy(-32768);
            decodeLookup[c] |= static_cast<LUTEntryTy>(
                static_cast<LUTUnsignedEntryTy>(Base::extend(diff, diff_l))
                << PayloadShift);
          }
        }
      }
    }
  }

  template <typename BIT_STREAM>
  __attribute__((always_inline)) int decodeCodeValue(BIT_STREAM& bs) const {
    static_assert(
        BitStreamerTraits<BIT_STREAM>::canUseWithPrefixCodeDecoder,
        "This BitStreamer specialization is not marked as usable here");
    invariant(!Base::isFullDecode());
    return decode<BIT_STREAM, false>(bs);
  }

  template <typename BIT_STREAM>
  __attribute__((always_inline)) int decodeDifference(BIT_STREAM& bs) const {
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
  __attribute__((always_inline)) int decode(BIT_STREAM& bs) const {
    static_assert(
        BitStreamerTraits<BIT_STREAM>::canUseWithPrefixCodeDecoder,
        "This BitStreamer specialization is not marked as usable here");
    invariant(FULL_DECODE == Base::isFullDecode());
    bs.fill(32);

    typename Base::CodeSymbol partial;
    partial.code_len = LookupDepth;
    partial.code = implicit_cast<typename Traits::CodeTy>(
        bs.peekBitsNoFill(partial.code_len));

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

    typename Traits::CodeValueTy codeValue;
    if (lutEntry) {
      // If the flag is not set, but the entry is not empty,
      // the payload is the code value for this symbol.
      partial.code_len = implicit_cast<uint8_t>(len);
      codeValue = implicit_cast<typename Traits::CodeValueTy>(payload);
      invariant(!FULL_DECODE || codeValue /*aka diff_l*/ > 0);
    } else {
      // No match in the lookup table, because either the code is longer
      // than LookupDepth or the input is corrupt. Need to read more bits...
      invariant(len == 0);
      bs.skipBitsNoFill(partial.code_len);
      std::tie(partial, codeValue) =
          Base::finishReadingPartialSymbol(bs, partial);
    }

    return Base::template processSymbol<BIT_STREAM, FULL_DECODE>(bs, partial,
                                                                 codeValue);
  }
};

} // namespace rawspeed
