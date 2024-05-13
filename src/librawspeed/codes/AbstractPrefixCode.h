/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2023 Roman Lebedev

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

#include "rawspeedconfig.h"
#include "adt/BitIterator.h"
#include "adt/iterator_range.h"
#include "decoders/RawDecoderException.h"
#include <cassert>
#include <cstdint>
#include <type_traits>
#include <vector>

namespace rawspeed {

template <typename CodeTag> struct CodeTraits final {
  // using CodeTy = uint<???>_t;
  // static constexpr int MaxCodeLenghtBits = <???>;
  // static constexpr int MaxNumCodeValues = <???>;

  // using CodeValueTy = uint<???>_t;
  // static constexpr int MaxCodeValueLenghtBits = <???>;
  // static constexpr CodeValueTy MaxCodeValue = <???>;

  // static constexpr int MaxDiffLengthBits = <???>;
  // static constexpr CodeValueTy MaxDiffLength = <???>;

  // static constexpr bool SupportsFullDecode = <???>;
};

struct BaselineCodeTag;

template <> struct CodeTraits<BaselineCodeTag> final {
  using CodeTy = uint16_t;
  static constexpr int MaxCodeLenghtBits = 16;
  static constexpr int MaxNumCodeValues = 162;

  using CodeValueTy = uint8_t;
  static constexpr int MaxCodeValueLenghtBits = 8;
  static constexpr CodeValueTy MaxCodeValue = 255;

  static constexpr int MaxDiffLengthBits = 5;
  static constexpr CodeValueTy MaxDiffLength = 16;

  static constexpr bool SupportsFullDecode = true;
};

struct VC5CodeTag;

template <> struct CodeTraits<VC5CodeTag> final {
  using CodeTy = uint32_t;
  static constexpr int MaxCodeLenghtBits = 26;
  static constexpr int MaxNumCodeValues = 264;

  using CodeValueTy = uint32_t;
  static constexpr int MaxCodeValueLenghtBits = 19;
  static constexpr CodeValueTy MaxCodeValue = 524287;

  static constexpr int MaxDiffLengthBits = -1;     // unused
  static constexpr CodeValueTy MaxDiffLength = -1; // unused

  static constexpr bool SupportsFullDecode = false;
};

template <typename CodeTag> struct CodeTraitsValidator final {
  using Traits = CodeTraits<CodeTag>;

  static_assert(std::is_integral_v<typename Traits::CodeTy>);
  static_assert(std::is_unsigned_v<typename Traits::CodeTy>);
  static_assert(std::is_same_v<typename Traits::CodeTy, uint16_t> ||
                std::is_same_v<typename Traits::CodeTy, uint32_t>);

  static_assert(Traits::MaxCodeLenghtBits > 0 &&
                Traits::MaxCodeLenghtBits <=
                    bitwidth<typename Traits::CodeTy>());
  static_assert(Traits::MaxCodeLenghtBits == 16 ||
                Traits::MaxCodeLenghtBits == 26);

  static_assert(Traits::MaxNumCodeValues > 0 &&
                Traits::MaxNumCodeValues <=
                    ((1ULL << Traits::MaxCodeLenghtBits) - 1ULL));
  static_assert(Traits::MaxNumCodeValues == 162 ||
                Traits::MaxNumCodeValues == 264);

  static_assert(std::is_integral_v<typename Traits::CodeValueTy>);
  static_assert(std::is_unsigned_v<typename Traits::CodeValueTy>);
  static_assert(std::is_same_v<typename Traits::CodeValueTy, uint8_t> ||
                std::is_same_v<typename Traits::CodeValueTy, uint32_t>);

  static_assert(Traits::MaxCodeValueLenghtBits > 0 &&
                Traits::MaxCodeValueLenghtBits <=
                    bitwidth<typename Traits::CodeValueTy>());
  static_assert(Traits::MaxCodeValueLenghtBits == 8 ||
                Traits::MaxCodeValueLenghtBits == 19);

  static_assert(Traits::MaxCodeValue > 0 &&
                Traits::MaxCodeValue <=
                    ((1ULL << Traits::MaxCodeValueLenghtBits) - 1ULL));
  static_assert(Traits::MaxCodeValue == 255 || Traits::MaxCodeValue == 524287);

  static_assert(
      std::is_same_v<decltype(Traits::SupportsFullDecode), const bool>);

  static_assert(!Traits::SupportsFullDecode ||
                (Traits::MaxDiffLengthBits > 0 &&
                 Traits::MaxDiffLengthBits <=
                     bitwidth<typename Traits::CodeValueTy>()));
  static_assert(!Traits::SupportsFullDecode ||
                (Traits::MaxDiffLengthBits == 5));

  static_assert(!Traits::SupportsFullDecode ||
                (Traits::MaxDiffLength > 0 &&
                 Traits::MaxDiffLength <=
                     ((1ULL << Traits::MaxDiffLengthBits) - 1ULL)));
  static_assert(!Traits::SupportsFullDecode || (Traits::MaxDiffLength == 16));

  static constexpr bool validate() { return true; }
};

template <typename CodeTag> class AbstractPrefixCode {
public:
  using Traits = CodeTraits<CodeTag>;
  using CodeValueTy = typename Traits::CodeValueTy;
  static_assert(CodeTraitsValidator<CodeTag>::validate());

  struct CodeSymbol final {
    typename Traits::CodeTy code; // the code (bit pattern)
    uint8_t code_len;             // the code length in bits

    CodeSymbol() = default;

    CodeSymbol(typename Traits::CodeTy code_, uint8_t code_len_)
        : code(code_), code_len(code_len_) {
      assert(code_len > 0);
      assert(code_len <= Traits::MaxCodeLenghtBits);
      assert(code <= ((1U << code_len) - 1U));
    }

    [[nodiscard]] iterator_range<BitMSBIterator<typename Traits::CodeTy>>
    getBitsMSB() const {
      return {{code, code_len - 1}, {code, -1}};
    }

    static bool HaveCommonPrefix(const CodeSymbol& symbol,
                                 const CodeSymbol& partial) {
      assert(partial.code_len <= symbol.code_len);

      const auto s0 = extractHighBits(symbol.code, partial.code_len,
                                      /*effectiveBitwidth=*/symbol.code_len);
      const auto s1 = partial.code;

      return s0 == s1;
    }

    bool RAWSPEED_READONLY operator==(const CodeSymbol& other) const {
      return code == other.code && code_len == other.code_len;
    }
  };

  AbstractPrefixCode() = default;

  explicit AbstractPrefixCode(std::vector<CodeValueTy> codeValues_)
      : codeValues(std::move(codeValues_)) {
    if (codeValues.empty())
      ThrowRDE("Empty code alphabet?");
    assert(
        all_of(codeValues.begin(), codeValues.end(),
               [](const CodeValueTy& v) { return v <= Traits::MaxCodeValue; }));
  }

  // The target alphabet, the values to which the (prefix) codes map, in order.
  std::vector<CodeValueTy> codeValues;
};

} // namespace rawspeed
