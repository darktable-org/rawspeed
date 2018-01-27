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

#include "common/Common.h"                // for ushort16, uchar8, int32
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "io/Buffer.h"                    // for Buffer
#include <algorithm>                      // for copy
#include <cassert>                        // for assert
#include <cstddef>                        // for size_t
#include <iterator>                       // for distance
#include <numeric>                        // for accumulate
#include <vector>                         // for vector, allocator, operator==

namespace rawspeed {

class AbstractHuffmanTable {
protected:
  inline size_t __attribute__((pure)) maxCodePlusDiffLength() const {
    return nCodesPerLength.size() - 1 +
           *(std::max_element(codeValues.cbegin(), codeValues.cend()));
  }

  // These two fields directly represent the contents of a JPEG DHT field

  // 1. The number of codes there are per bit length, this is index 1 based.
  // (there are always 0 codes of length 0)
  std::vector<unsigned int> nCodesPerLength; // index is length of code

  inline unsigned int __attribute__((pure)) maxCodesCount() const {
    return std::accumulate(nCodesPerLength.begin(), nCodesPerLength.end(), 0U);
  }

  // 2. This is the actual huffman encoded data, i.e. the 'alphabet'. Each value
  // is the number of bits following the code that encode the difference to the
  // last pixel. Valid values are in the range 0..16.
  // signExtended() is used to decode the difference bits to a signed int.
  std::vector<uchar8> codeValues; // index is just sequential number

public:
  bool operator==(const AbstractHuffmanTable& other) const {
    return nCodesPerLength == other.nCodesPerLength &&
           codeValues == other.codeValues;
  }

  uint32 setNCodesPerLength(const Buffer& data) {
    assert(data.getSize() == 16);

    nCodesPerLength.resize(17, 0);
    std::copy(data.begin(), data.end(), &nCodesPerLength[1]);
    assert(nCodesPerLength[0] == 0);

    // trim empty entries from the codes per length table on the right
    while (!nCodesPerLength.empty() && nCodesPerLength.back() == 0)
      nCodesPerLength.pop_back();

    if (nCodesPerLength.empty())
      ThrowRDE("Codes-per-length table is empty");

    assert(nCodesPerLength.back() > 0);

    const auto count = maxCodesCount();
    assert(count > 0);

    if (count > 162)
      ThrowRDE("Too big code-values table");

    for (auto codeLen = 1U; codeLen < nCodesPerLength.size(); codeLen++) {
      // we have codeLen bits. make sure that that code count can actually fit
      const auto nCodes = nCodesPerLength[codeLen];
      if (nCodes > ((1U << codeLen) - 1U)) {
        ThrowRDE("Corrupt Huffman. Can not have %u codes in %u-bit len", nCodes,
                 codeLen);
      }
    }

    return count;
  }

  void setCodeValues(const Buffer& data) {
    // spec says max 16 but Hasselblad ignores that -> allow 17
    // Canon's old CRW really ignores this ...
    assert(data.getSize() <= 162);
    assert(data.getSize() == maxCodesCount());

    codeValues.clear();
    codeValues.reserve(maxCodesCount());
    std::copy(data.begin(), data.end(), std::back_inserter(codeValues));
    assert(codeValues.size() == maxCodesCount());

    for (const auto cValue : codeValues) {
      if (cValue > 16)
        ThrowRDE("Corrupt Huffman. Code value %u is bigger than 16", cValue);
    }
  }

  // WARNING: the caller should check that len != 0 before calling the function
  inline static int __attribute__((const))
  signExtended(uint32 diff, uint32 len) {
    int32 ret = diff;
#if 0
#define _X(x) (1 << x) - 1
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
};

} // namespace rawspeed
