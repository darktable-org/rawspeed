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

class HuffmanTable final : public AbstractHuffmanTable {
  // private fields calculated from codesPerBits and codeValues
  // they are index '1' based, so we can directly lookup the value
  // for code length l without decrementing
  std::vector<ushort16> maxCodeOL;    // index is length of code
  std::vector<ushort16> codeOffsetOL; // index is length of code

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
  std::vector<int32> decodeLookup;
#else
  // lookup table containing 2 fields: payload:4|len:4
  // the payload is the length of the diff, len is the length of the code
  static constexpr unsigned LookupDepth = 15;
  static constexpr unsigned PayloadShift = 4;
  static constexpr unsigned FlagMask = 0;
  static constexpr unsigned LenMask = 0x0f;
  std::vector<uchar8> decodeLookup;
#endif

  bool fullDecode = true;
  bool fixDNGBug16 = false;

public:
  void setup(bool fullDecode_, bool fixDNGBug16_) {
    this->fullDecode = fullDecode_;
    this->fixDNGBug16 = fixDNGBug16_;

    // store the code lengths in bits, valid values are 0..16
    std::vector<uchar8> code_len; // index is just sequential number
    // store the codes themselves (bit patterns found inside the stream)
    std::vector<ushort16> codes;  // index is just sequential number

    assert(!nCodesPerLength.empty());
    assert(maxCodesCount() > 0);

    unsigned int maxCodeLength = nCodesPerLength.size() - 1U;
    assert(codeValues.size() == maxCodesCount());

    assert(maxCodePlusDiffLength() <= 32U);

    // reserve all the memory. avoids lots of small allocs
    code_len.reserve(maxCodesCount());
    codes.reserve(maxCodesCount());

    // Figure C.1: make table of Huffman code length for each symbol
    // Figure C.2: generate the codes themselves
    uint32 code = 0;
    for (unsigned int l = 1; l <= maxCodeLength; ++l) {
      assert(nCodesPerLength[l] <= ((1U << l) - 1U));

      for (unsigned int i = 0; i < nCodesPerLength[l]; ++i) {
        if (code > 0xffff) {
          ThrowRDE("Corrupt Huffman: code value overflow on len = %u, %u-th "
                   "code out of %u\n",
                   l, i, nCodesPerLength[l]);
        }

        code_len.push_back(l);
        codes.push_back(code);
        code++;
      }
      code <<= 1;
    }

    assert(code_len.size() == maxCodesCount());
    assert(codes.size() == maxCodesCount());

    // Figure F.15: generate decoding tables
    codeOffsetOL.resize(maxCodeLength + 1UL, 0xffff);
    maxCodeOL.resize(maxCodeLength + 1UL);
    int code_index = 0;
    for (unsigned int l = 1U; l <= maxCodeLength; l++) {
      if (nCodesPerLength[l]) {
        codeOffsetOL[l] = codes[code_index] - code_index;
        code_index += nCodesPerLength[l];
        maxCodeOL[l] = codes[code_index - 1];
      }
    }

    // Generate lookup table for fast decoding lookup.
    // See definition of decodeLookup above
    decodeLookup.resize(1 << LookupDepth);
    for (size_t i = 0; i < codes.size(); i++) {
      uchar8 code_l = code_len[i];
      if (code_l > static_cast<int>(LookupDepth))
        break;

      ushort16 ll = codes[i] << (LookupDepth - code_l);
      ushort16 ul = ll | ((1 << (LookupDepth - code_l)) - 1);
      ushort16 diff_l = codeValues[i];
      for (ushort16 c = ll; c <= ul; c++) {
        if (!(c < decodeLookup.size()))
          ThrowRDE("Corrupt Huffman");

        if (!FlagMask || !fullDecode || diff_l + code_l > LookupDepth) {
          // lookup bit depth is too small to fit both the encoded length
          // and the final difference value.
          // -> store only the length and do a normal sign extension later
          decodeLookup[c] = diff_l << PayloadShift | code_l;
        } else {
          // diff_l + code_l <= lookupDepth
          // The table bit depth is large enough to store both.
          decodeLookup[c] = (code_l + diff_l) | FlagMask;

          if (diff_l) {
            uint32 diff = (c >> (LookupDepth - code_l - diff_l)) & ((1 << diff_l) - 1);
            decodeLookup[c] |= static_cast<uint32>(signExtended(diff, diff_l))
                               << PayloadShift;
          }
        }
      }
    }
  }

  template<typename BIT_STREAM> inline int decodeLength(BIT_STREAM& bs) const {
    assert(!fullDecode);
    return decode<BIT_STREAM, false>(bs);
  }

  template<typename BIT_STREAM> inline int decodeNext(BIT_STREAM& bs) const {
    assert(fullDecode);
    return decode<BIT_STREAM, true>(bs);
  }

  // The bool template paraeter is to enable two versions:
  // one returning only the length of the of diff bits (see Hasselblad),
  // one to return the fully decoded diff.
  // All ifs depending on this bool will be optimized out by the compiler
  template<typename BIT_STREAM, bool FULL_DECODE> inline int decode(BIT_STREAM& bs) const {
    assert(FULL_DECODE == fullDecode);

    // 32 is the absolute maximum combined length of code + diff
    // assertion  maxCodePlusDiffLength() <= 32U  is already checked in setup()
    bs.fill(32);

    // for processors supporting bmi2 instructions, using maxCodePlusDiffLength()
    // might be benifitial

    uint32 code = bs.peekBitsNoFill(LookupDepth);
    assert(code < decodeLookup.size());
    int val = decodeLookup[code];
    int len = val & LenMask;
    assert(len >= 0);
    assert(len <= 16);

    // if the code is invalid (bitstream corrupted) len will be 0
    bs.skipBitsNoFill(len);
    if (FULL_DECODE && val & FlagMask) {
      // if the flag bit is set, the payload is the already sign extended difference
      return val >> PayloadShift;
    }

    if (len) {
      // if the flag bit is not set but len != 0, the payload is the number of bits to sign extend and return
      const int l_diff = val >> PayloadShift;
      assert((FULL_DECODE && (len + l_diff <= 32)) || !FULL_DECODE);
      return FULL_DECODE ? signExtended(bs.getBitsNoFill(l_diff), l_diff) : l_diff;
    }

    uint32 code_l = LookupDepth;
    bs.skipBitsNoFill(code_l);
    while (code_l < maxCodeOL.size() && code > maxCodeOL[code_l]) {
      uint32 temp = bs.getBitsNoFill(1);
      code = (code << 1) | temp;
      code_l++;
    }

    if (code_l >= maxCodeOL.size() || code > maxCodeOL[code_l])
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
    assert((diff_l && (len + code_l + diff_l <= 32)) || !diff_l);
    return diff_l ? signExtended(bs.getBitsNoFill(diff_l), diff_l) : 0;
  }
};

} // namespace rawspeed
