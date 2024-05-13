/*
    RawSpeed - RAW file decoder.

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

#include "adt/Array1DRef.h"
#include "adt/Optional.h"
#include "codes/AbstractPrefixCode.h"
#include "codes/HuffmanCode.h"
#include "codes/PrefixCode.h"
#include "common/RawspeedException.h"
#include "io/Buffer.h"
#include "io/ByteStream.h"
#include <cassert>
#include <cstdint>
#include <type_traits>
#include <vector>

template <typename CodeTag>
auto getCodeValues(rawspeed::ByteStream& bs, unsigned numEntries) {
  using Traits = rawspeed::CodeTraits<CodeTag>;
  using CodeValueTy = typename Traits::CodeValueTy;

  std::vector<CodeValueTy> values;
  values.reserve(numEntries);
  std::generate_n(std::back_inserter(values), numEntries, [&bs]() {
    auto code = bs.get<CodeValueTy>();
    if (!(code <= Traits::MaxCodeValue))
      ThrowRSE("Bad code value");
    return code;
  });
  assert(values.size() == numEntries);
  return values;
}

template <typename CodeTag>
auto getCodeSymbols(rawspeed::ByteStream& bs, unsigned numSymbols) {
  using Traits = rawspeed::CodeTraits<CodeTag>;
  using CodeSymbol = typename rawspeed::AbstractPrefixCode<CodeTag>::CodeSymbol;

  std::vector<CodeSymbol> symbols;
  symbols.reserve(numSymbols);
  std::generate_n(
      std::back_inserter(symbols), numSymbols, [&bs]() -> CodeSymbol {
        auto code_len = bs.get<uint8_t>();
        if (!(code_len > 0 && code_len <= Traits::MaxCodeLenghtBits))
          ThrowRSE("Bad code length");
        auto code = bs.get<typename Traits::CodeTy>();
        if (!(code <= ((1U << code_len) - 1U)))
          ThrowRSE("Bad code");
        return {code, code_len};
      });
  assert(symbols.size() == numSymbols);
  return symbols;
}

template <typename CodeTag>
inline rawspeed::HuffmanCode<CodeTag>
createHuffmanCode(rawspeed::ByteStream& bs) {
  using Traits = rawspeed::CodeTraits<CodeTag>;

  rawspeed::HuffmanCode<CodeTag> hc;

  // first bytes are consumed as n-codes-per-length
  const auto count =
      hc.setNCodesPerLength(bs.getBuffer(Traits::MaxCodeLenghtBits));

  if (count) {
    // and then count more bytes consumed as code values
    rawspeed::ByteStream codeValuesStream =
        bs.getStream(count, sizeof(typename Traits::CodeValueTy));
    const auto codesBuf = getCodeValues<CodeTag>(codeValuesStream, count);
    hc.setCodeValues(rawspeed::Array1DRef<const typename Traits::CodeValueTy>(
        codesBuf.data(),
        rawspeed::implicit_cast<rawspeed::Buffer::size_type>(codesBuf.size())));
  }

  return hc;
}

template <typename CodeTag>
inline rawspeed::PrefixCode<CodeTag>
createPrefixCode(rawspeed::ByteStream& bs) {
  using Traits = rawspeed::CodeTraits<CodeTag>;

  unsigned numCodeValues = bs.getU32();
  unsigned numSymbols = bs.getU32();

  rawspeed::ByteStream codeValuesStream =
      bs.getStream(numCodeValues, sizeof(typename Traits::CodeValueTy));
  rawspeed::ByteStream symbolsStream = bs.getStream(
      numSymbols, sizeof(uint8_t) + sizeof(typename Traits::CodeTy));

  auto codeValues = getCodeValues<CodeTag>(codeValuesStream, numCodeValues);
  auto symbols = getCodeSymbols<CodeTag>(symbolsStream, numSymbols);

  return {symbols, codeValues};
}

template <typename T, typename CodeTag>
  requires std::is_constructible_v<T, rawspeed::HuffmanCode<CodeTag>>
inline T createHuffmanPrefixCodeDecoderImpl(rawspeed::ByteStream& bs) {
  auto hc = createHuffmanCode<CodeTag>(bs);
  return T(std::move(hc));
}

template <typename T, typename CodeTag>
  requires(!std::is_constructible_v<T, rawspeed::HuffmanCode<CodeTag>>)
inline T createHuffmanPrefixCodeDecoderImpl(rawspeed::ByteStream& bs) {
  auto hc = createHuffmanCode<CodeTag>(bs);
  auto code = hc.operator rawspeed::PrefixCode<CodeTag>();
  return T(std::move(code));
}

template <typename T, typename CodeTag>
  requires(!std::is_constructible_v<T, rawspeed::PrefixCode<CodeTag>>)
inline T createSimplePrefixCodeDecoderImpl(rawspeed::ByteStream& bs) {
  ThrowRSE(
      "This Prefix code decoder implementation only support Huffman codes");
}

template <typename T, typename CodeTag>
  requires std::is_constructible_v<T, rawspeed::PrefixCode<CodeTag>>
inline T createSimplePrefixCodeDecoderImpl(rawspeed::ByteStream& bs) {
  auto pc = createPrefixCode<CodeTag>(bs);
  return T(std::move(pc));
}

template <typename T>
static T createPrefixCodeDecoder(rawspeed::ByteStream& bs) {
  using CodeTag = typename T::Tag;

  rawspeed::Optional<T> ht;
  if (bool huffmanCode = bs.getByte() != 0; huffmanCode)
    ht = createHuffmanPrefixCodeDecoderImpl<T, CodeTag>(bs);
  else
    ht = createSimplePrefixCodeDecoderImpl<T, CodeTag>(bs);

  // and one more byte as 'fixDNGBug16' boolean
  const bool fixDNGBug16 = bs.getByte() != 0;

  bool fullDecode = false;
  if constexpr (T::Traits::SupportsFullDecode)
    fullDecode = bs.getByte() != 0;

  ht->setup(fullDecode, fixDNGBug16);

  return std::move(*ht);
}
