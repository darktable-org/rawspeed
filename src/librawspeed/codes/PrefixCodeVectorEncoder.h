/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2024 Roman Lebedev

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
#include "bitstreams/BitVacuumer.h"
#include "codes/AbstractPrefixCodeEncoder.h"

namespace rawspeed {

template <typename CodeTag>
class PrefixCodeVectorEncoder : public AbstractPrefixCodeEncoder<CodeTag> {
public:
  using Tag = CodeTag;
  using Base = AbstractPrefixCodeEncoder<CodeTag>;
  using Traits = typename Base::Traits;

  using Base::Base;

private:
  template <typename BIT_VACUUMER>
  void encodeCodeValueImpl(BIT_VACUUMER& bv, int codeIndex) const {
    static_assert(
        BitVacuumerTraits<BIT_VACUUMER>::canUseWithPrefixCodeEncoder,
        "This BitVacuumer specialization is not marked as usable here");
    invariant(codeIndex >= 0);
    const auto numCodeSymbols = implicit_cast<int>(Base::code.symbols.size());
    invariant(codeIndex < numCodeSymbols);
    const typename Base::CodeSymbol& symbol = Base::code.symbols[codeIndex];
    bv.put(symbol.code, symbol.code_len);
  }

  [[nodiscard]] int
  getCodeIndexOfCodeValue(const typename Traits::CodeValueTy value) const {
    for (int codeIndex = 0;
         codeIndex != implicit_cast<int>(Base::code.codeValues.size());
         ++codeIndex) {
      const auto& codeValue = Base::code.codeValues[codeIndex];
      if (codeValue == value)
        return codeIndex;
    }
    __builtin_unreachable();
  }

public:
  void setup(bool fullDecode_, bool fixDNGBug16_) {
    AbstractPrefixCodeEncoder<CodeTag>::setup(fullDecode_, fixDNGBug16_);
  }

  template <typename BIT_VACUUMER>
  void encodeCodeValue(BIT_VACUUMER& bv,
                       typename Traits::CodeValueTy codeValue) const {
    static_assert(
        BitVacuumerTraits<BIT_VACUUMER>::canUseWithPrefixCodeEncoder,
        "This BitVacuumer specialization is not marked as usable here");
    invariant(!Base::isFullDecode());
    int codeIndex = getCodeIndexOfCodeValue(codeValue);
    encodeCodeValueImpl(bv, codeIndex);
  }

  template <typename BIT_VACUUMER>
  void encodeDifference(BIT_VACUUMER& bv, int value) const {
    static_assert(
        BitVacuumerTraits<BIT_VACUUMER>::canUseWithPrefixCodeEncoder,
        "This BitVacuumer specialization is not marked as usable here");
    invariant(Base::isFullDecode());
    auto [diff, diffLen] = Base::reduce(value);
    int codeIndex = getCodeIndexOfCodeValue(diffLen);
    encodeCodeValueImpl(bv, codeIndex);
    if (diffLen != 16 || Base::handleDNGBug16())
      bv.put(diff, diffLen);
  }

  template <typename BIT_VACUUMER, bool FULL_DECODE>
  void encode(BIT_VACUUMER& bv, int value) const {
    static_assert(
        BitVacuumerTraits<BIT_VACUUMER>::canUseWithPrefixCodeEncoder,
        "This BitVacuumer specialization is not marked as usable here");
    invariant(FULL_DECODE == Base::isFullDecode());

    if constexpr (!FULL_DECODE)
      encodeCodeValue(bv, implicit_cast<typename Traits::CodeValueTy>(value));
    else
      encodeDifference(bv, value);
  }
};

} // namespace rawspeed
