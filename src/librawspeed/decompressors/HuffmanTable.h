/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2017 Axel Waggershauser

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

#include "common/Common.h"                // for ushort16, uchar8, int32
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "io/Buffer.h"                    // for Buffer
#include <algorithm>                      // for copy
#include <cassert>                        // for assert
#include <cstddef>                        // for size_t
#include <numeric>                        // for accumulate
#include <vector>                         // for vector, allocator, operator==

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

namespace RawSpeed {

class HuffmanTable
{
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

  bool fixDNGBug16 = false;

  size_t maxCodePlusDiffLength() const {
    return nCodesPerLength.size()-1 + codeValues.size()-1;
  }

public:

  // These two fields directly represent the contents of a JPEG DHT field
  // 1. The number of codes there are per bit length, this is index 1 based.
  // (there are always 0 codes of length 0)
  std::vector<int> nCodesPerLength; // index is length of code
  // 2. This is the actual huffman encoded data, i.e. the 'alphabet'. Each value
  // is the number of bits following the code that encode the difference to the
  // last pixel. Valid values are in the range 0..16.
  // signExtended() is used to decode the difference bits to a signed int.
  std::vector<uchar8> codeValues;   // index is just sequential number

  bool operator==(const HuffmanTable& other) const {
    return nCodesPerLength == other.nCodesPerLength
        && codeValues      == other.codeValues;
  }

  uint32 setNCodesPerLength(const Buffer& data) {
    assert(data.getSize() == 16);
    nCodesPerLength.resize(17);
    std::copy(data.begin(), data.end(), &nCodesPerLength[1]);
    // trim empty entries from the codes per length table on the right
    while (nCodesPerLength.back() == 0)
      nCodesPerLength.pop_back();
    return std::accumulate(data.begin(), data.end(), 0);
  }

  void setCodeValues(const Buffer& data) {
    // spec says max 16 but Hasselblad ignores that -> allow 17
    // Canon's old CRW really ignores this ...
    assert(data.getSize() <= 162);
    codeValues.assign(data.begin(), data.end());
  }

  void setup(bool fullDecode, bool fixDNGBug16_) {
    this->fixDNGBug16 = fixDNGBug16_;

    // store the code lengths in bits, valid values are 0..16
    std::vector<uchar8> code_len; // index is just sequential number
    // store the codes themselfs (bit patterns found inside the stream)
    std::vector<ushort16> codes;  // index is just sequential number

    int maxCodeLength = nCodesPerLength.size()-1;

    // precompute how much code entries there are
    size_t maxCodesCount = 0;
    for (int l = 1; l <= maxCodeLength; ++l) {
      for (int i = 0; i < nCodesPerLength[l]; ++i) {
        maxCodesCount++;
      }
    }

    // reserve all the memory. avoids lots of small allocs
    code_len.reserve(maxCodesCount);
    codes.reserve(maxCodesCount);

    // Figure C.1: make table of Huffman code length for each symbol
    // Figure C.2: generate the codes themselves
    uint32 code = 0;
    for (int l = 1; l <= maxCodeLength; ++l) {
      assert(nCodesPerLength[l] < (1<<l));
      for (int i = 0; i < nCodesPerLength[l]; ++i) {
        assert(code <= 0xffff);
        code_len.push_back(l);
        codes.push_back(code++);
      }
      code <<= 1;
    }

    assert(code_len.size() == maxCodesCount);
    assert(codes.size() == maxCodesCount);

    // Figure F.15: generate decoding tables
    codeOffsetOL.resize(maxCodeLength + 1UL, 0xffff);
    maxCodeOL.resize(maxCodeLength + 1UL);
    int code_index = 0;
    for (int l = 1; l <= maxCodeLength; l++) {
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
      if (code_l > (int)LookupDepth)
        break;

      ushort16 ll = codes[i] << (LookupDepth - code_l);
      ushort16 ul = ll | ((1 << (LookupDepth - code_l)) - 1);
      ushort16 diff_l = codeValues[i];
      for (ushort16 c = ll; c <= ul; c++) {
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
            decodeLookup[c] |= (uint32)signExtended(diff, diff_l) << PayloadShift;
          }
        }
      }
    }
  }

  // WARNING: the caller should check that len != 0 before calling the function
  inline static int __attribute__((const))
  signExtended(uint32 diff, uint32 len) {
    int32 ret = diff;
#if 0
#define _X(x) (1<<x)-1
    constexpr static int offset[16] = {
      0,     _X(1), _X(2),  _X(3),  _X(4),  _X(5),  _X(6),  _X(7),
      _X(8), _X(9), _X(10), _X(11), _X(12), _X(13), _X(14), _X(15)};
#undef _X
    if ((diff & (1 << (len - 1))) == 0)
      ret -= offset[len];
#else
    if ((diff & (1 << (len - 1))) == 0)
      ret -= (1 << len) - 1;
#endif
    return ret;
  }

  template<typename BIT_STREAM> inline int decodeLength(BIT_STREAM& bs) const {
    return decode<BIT_STREAM, false>(bs);
  }

  template<typename BIT_STREAM> inline int decodeNext(BIT_STREAM& bs) const {
    return decode<BIT_STREAM, true>(bs);
  }

  // The bool template paraeter is to enable two versions:
  // one returning only the length of the of diff bits (see Hasselblad),
  // one to return the fully decoded diff.
  // All ifs depending on this bool will be optimized out by the compiler
  template<typename BIT_STREAM, bool FULL_DECODE> inline int decode(BIT_STREAM& bs) const {
    // 32 is the absolute maximum combined length of code + diff
    // for processors supporting bmi2 instructions, using maxCodePlusDiffLength()
    // might be benifitial
    bs.fill(32);
    uint32 code = bs.peekBitsNoFill(LookupDepth);

    int val = decodeLookup[code];
    int len = val & LenMask;
    // if the code is invalid (bitstream corrupted) len will be 0
    bs.skipBitsNoFill(len);
    if (FULL_DECODE && val & FlagMask) {
      // if the flag bit is set, the payload is the already sign extended difference
      return val >> PayloadShift;
    }

    if (len) {
      // if the flag bit is not set but len != 0, the payload is the number of bits to sign extend and return
      int l_diff = val >> PayloadShift;
      return FULL_DECODE ? signExtended(bs.getBitsNoFill(l_diff), l_diff) : l_diff;
    }

    uint32 code_l = LookupDepth;
    bs.skipBitsNoFill(code_l);
    while (code_l < maxCodeOL.size() && code > maxCodeOL[code_l]) {
      uint32 temp = bs.getBitsNoFill(1);
      code = (code << 1) | temp;
      code_l++;
    }

    if (code > maxCodeOL[code_l])
      ThrowRDE("bad Huffman code: %u (len: %u)", code, code_l);

    int diff_l = codeValues[code - codeOffsetOL[code_l]];

    if (!FULL_DECODE)
      return diff_l;

    if (diff_l == 16) {
      if (fixDNGBug16)
        bs.skipBits(16);
      return -32768;
    }

    return diff_l ? signExtended(bs.getBitsNoFill(diff_l), diff_l) : 0;
  }
};

} // namespace RawSpeed
