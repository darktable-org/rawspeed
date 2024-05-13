/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2017 Axel Waggershauser
    Copyright (C) 2018 Roman Lebedev

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
#include "adt/Optional.h"
#include "bitstreams/BitStreamer.h"
#include "codes/AbstractPrefixCodeDecoder.h"
#include "codes/BinaryPrefixTree.h"
#include "decoders/RawDecoderException.h"
#include <cassert>
#include <tuple>
#include <utility>

namespace rawspeed {

template <typename CodeTag>
class PrefixCodeTreeDecoder : public AbstractPrefixCodeDecoder<CodeTag> {
public:
  using Tag = CodeTag;
  using Base = AbstractPrefixCodeDecoder<CodeTag>;
  using Traits = typename Base::Traits;

  using Base::Base;

private:
  BinaryPrefixTree<CodeTag> tree;

protected:
  template <typename BIT_STREAM>
  std::pair<typename Base::CodeSymbol, int /*codeValue*/>
  finishReadingPartialSymbol(BIT_STREAM& bs,
                             typename Base::CodeSymbol initialPartial) const {
    typename Base::CodeSymbol partial;
    partial.code = 0;
    partial.code_len = 0;

    const auto* top = &(tree.root->getAsBranch());

    auto walkBinaryTree = [&partial, &top](bool bit)
        -> Optional<std::pair<typename Base::CodeSymbol, int /*codeValue*/>> {
      partial.code <<= 1;
      partial.code |= bit;
      partial.code_len++;

      // NOTE: The order *IS* important! Left to right, zero to one!
      const auto& newNode = top->buds[bit];

      if (!newNode) {
        // Got nothing in this direction.
        ThrowRDE("bad Huffman code: %u (len: %u)", partial.code,
                 partial.code_len);
      }

      if (static_cast<typename decltype(tree)::Node::Type>(*newNode) ==
          decltype(tree)::Node::Type::Leaf) {
        // Ok, great, hit a Leaf. This is it.
        return {{partial, newNode->getAsLeaf().value}};
      }

      // Else, this is a branch, continue looking.
      top = &(newNode->getAsBranch());
      return std::nullopt;
    };

    // First, translate pre-existing code bits.
    for (unsigned bit : initialPartial.getBitsMSB()) {
      if (auto sym = walkBinaryTree(bit))
        return *sym;
    }

    // Read bits until either find the code or detect the incorrect code
    while (true) {
      invariant(partial.code_len <= Traits::MaxCodeLenghtBits);

      // Read one more bit
      const bool bit = bs.getBitsNoFill(1);

      if (auto sym = walkBinaryTree(bit))
        return *sym;
    }

    // We have either returned the found symbol, or thrown on incorrect symbol.
    __builtin_unreachable();
  }

  template <typename BIT_STREAM>
  std::pair<typename Base::CodeSymbol, int /*codeValue*/>
  readSymbol(BIT_STREAM& bs) const {
    static_assert(
        BitStreamerTraits<BIT_STREAM>::canUseWithPrefixCodeDecoder,
        "This BitStreamer specialization is not marked as usable here");

    // Start from completely unknown symbol.
    typename Base::CodeSymbol partial;
    partial.code_len = 0;
    partial.code = 0;

    return finishReadingPartialSymbol(bs, partial);
  }

public:
  void setup(bool fullDecode_, bool fixDNGBug16_) {
    AbstractPrefixCodeDecoder<CodeTag>::setup(fullDecode_, fixDNGBug16_);

    assert(Base::code.symbols.size() == Base::code.codeValues.size());
    for (unsigned codeIndex = 0; codeIndex != Base::code.symbols.size();
         ++codeIndex)
      tree.add(Base::code.symbols[codeIndex], Base::code.codeValues[codeIndex]);
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

  // The bool template paraeter is to enable two versions:
  // one returning only the length of the of diff bits (see Hasselblad),
  // one to return the fully decoded diff.
  // All ifs depending on this bool will be optimized out by the compiler
  template <typename BIT_STREAM, bool FULL_DECODE>
  int decode(BIT_STREAM& bs) const {
    static_assert(
        BitStreamerTraits<BIT_STREAM>::canUseWithPrefixCodeDecoder,
        "This BitStreamer specialization is not marked as usable here");
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
