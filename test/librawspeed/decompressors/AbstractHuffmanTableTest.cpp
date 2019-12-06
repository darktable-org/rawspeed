/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2018 Roman Lebedev

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; withexpected even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include "decompressors/AbstractHuffmanTable.h" // for AbstractHuffmanTable...
#include "common/Common.h"                      // for uint8_t, uint32_t
#include "io/Buffer.h"                          // for Buffer
#include <algorithm>                            // for min
#include <bitset>                               // for bitset
#include <cassert>                              // for assert
#include <cstdlib>                              // for exit
#include <gtest/gtest.h>                        // for make_tuple, Message
#include <initializer_list>                     // for initializer_list<>::...
#include <ostream>                              // for operator<<, ostream
#include <string>                               // for basic_string, operat...
#include <utility>                              // for move
#include <vector>                               // for vector

using rawspeed::AbstractHuffmanTable;
using rawspeed::Buffer;
using std::make_tuple;

namespace rawspeed {

class RawDecoderException;

bool operator!=(const AbstractHuffmanTable& lhs,
                const AbstractHuffmanTable& rhs) {
  return !(lhs == rhs);
}

::std::ostream& operator<<(::std::ostream& os,
                           const AbstractHuffmanTable::CodeSymbol s) {
  auto str = std::bitset<32>(s.code).to_string();

  str = str.substr(str.size() - s.code_len);
  return os << "0b" << str;
}

bool operator!=(const AbstractHuffmanTable::CodeSymbol& lhs,
                const AbstractHuffmanTable::CodeSymbol& rhs) {
  return !(lhs == rhs);
}

} // namespace rawspeed

namespace rawspeed_test {

TEST(AbstractHuffmanTableCodeSymbolTest, Equality) {
#define s AbstractHuffmanTable::CodeSymbol
  ASSERT_EQ(s(0, 1), s(0, 1));
  ASSERT_EQ(s(1, 1), s(1, 1));

  ASSERT_NE(s(1, 1), s(0, 1));
  ASSERT_NE(s(0, 1), s(1, 1));
#undef s
}

#ifndef NDEBUG
TEST(CodeSymbolDeathTest, CodeSymbolLength) {
  ASSERT_DEATH({ AbstractHuffmanTable::CodeSymbol(0, 0); }, "code_len > 0");
  ASSERT_DEATH({ AbstractHuffmanTable::CodeSymbol(1, 0); }, "code_len > 0");
  ASSERT_DEATH({ AbstractHuffmanTable::CodeSymbol(0, 17); }, "code_len <= 16");
  ASSERT_DEATH({ AbstractHuffmanTable::CodeSymbol(1, 17); }, "code_len <= 16");
}

using CodeSymbolType = std::tuple<int, int, bool>;
class CodeSymbolDeathTest : public ::testing::TestWithParam<CodeSymbolType> {
protected:
  CodeSymbolDeathTest() = default;
  virtual void SetUp() {
    auto p = GetParam();

    val = std::get<0>(p);
    len = std::get<1>(p);
    die = std::get<2>(p);
  }

  int val;
  int len;
  bool die;
};
static const CodeSymbolType CodeSymbolData[]{
    // clang-format off
    make_tuple(0b00, 1, false),
    make_tuple(0b00, 2, false),
    make_tuple(0b01, 1, false),
    make_tuple(0b01, 2, false),
    make_tuple(0b10, 1, true),
    make_tuple(0b10, 2, false),
    make_tuple(0b11, 1, true),
    make_tuple(0b11, 2, false),
    // clang-format on
};
INSTANTIATE_TEST_CASE_P(CodeSymbolDeathTest, CodeSymbolDeathTest,
                        ::testing::ValuesIn(CodeSymbolData));
TEST_P(CodeSymbolDeathTest, CodeSymbolDeathTest) {
  if (die) {
    ASSERT_DEATH({ AbstractHuffmanTable::CodeSymbol(val, len); },
                 "code <= \\(\\(1U << code_len\\) - 1U\\)");
  } else {
    ASSERT_EXIT(
        {
          AbstractHuffmanTable::CodeSymbol(val, len);
          exit(0);
        },
        ::testing::ExitedWithCode(0), "");
  }
}
#endif

using CodeSymbolPrintDataType = std::tuple<int, int, std::string>;
class CodeSymbolPrintTest
    : public ::testing::TestWithParam<CodeSymbolPrintDataType> {
protected:
  CodeSymbolPrintTest() = default;
  virtual void SetUp() {
    auto p = GetParam();

    val = std::get<0>(p);
    len = std::get<1>(p);
    str = std::get<2>(p);
  }

  int val;
  int len;
  std::string str;
};
static const CodeSymbolPrintDataType CodeSymbolPrintData[]{
    // clang-format off
    make_tuple(0b00, 1, "0b0"),
    make_tuple(0b00, 2, "0b00"),
    make_tuple(0b01, 1, "0b1"),
    make_tuple(0b01, 2, "0b01"),
    make_tuple(0b10, 2, "0b10"),
    make_tuple(0b11, 2, "0b11"),
    // clang-format on
};
INSTANTIATE_TEST_CASE_P(CodeSymbolPrintTest, CodeSymbolPrintTest,
                        ::testing::ValuesIn(CodeSymbolPrintData));
TEST_P(CodeSymbolPrintTest, CodeSymbolPrintTest) {
  ASSERT_EQ(
      ::testing::PrintToString(AbstractHuffmanTable::CodeSymbol(val, len)),
      str);
}

using CodeSymbolHaveCommonPrefixDataType =
    std::tuple<AbstractHuffmanTable::CodeSymbol,
               AbstractHuffmanTable::CodeSymbol>;
class CodeSymbolHaveCommonPrefixTest
    : public ::testing::TestWithParam<CodeSymbolHaveCommonPrefixDataType> {
protected:
  CodeSymbolHaveCommonPrefixTest() = default;
  virtual void SetUp() {
    auto p = GetParam();

    symbol = std::get<0>(p);
    partial = std::get<1>(p);
  }

  AbstractHuffmanTable::CodeSymbol symbol;
  AbstractHuffmanTable::CodeSymbol partial;
};
std::vector<AbstractHuffmanTable::CodeSymbol> GenerateAllPossibleCodeSymbols() {
  // change those two together
  static constexpr auto maxLen = 2U;
  static constexpr auto expectedCnt = 2U + 4U;

  std::vector<AbstractHuffmanTable::CodeSymbol> allVariants;
  allVariants.reserve(expectedCnt);
  for (unsigned l = 1; l <= maxLen; l++) {
    for (unsigned c = 0; c <= ((1U << l) - 1U); c++)
      allVariants.emplace_back(c, l);
  }
  assert(allVariants.size() == expectedCnt);
  return allVariants;
}
static const auto allPossibleCodeSymbols = GenerateAllPossibleCodeSymbols();
INSTANTIATE_TEST_CASE_P(
    CodeSymbolHaveCommonPrefixTest, CodeSymbolHaveCommonPrefixTest,
    ::testing::Combine(::testing::ValuesIn(allPossibleCodeSymbols),
                       ::testing::ValuesIn(allPossibleCodeSymbols)));
TEST_P(CodeSymbolHaveCommonPrefixTest, CodeSymbolHaveCommonPrefixTest) {
  if (partial.code_len > symbol.code_len)
    return;

  auto symbol_str = ::testing::PrintToString(symbol);
  auto partial_str = ::testing::PrintToString(partial);
  const auto len = std::min(symbol_str.length(), partial_str.length());
  // Trim them to the same length (cut end chars)
  symbol_str.resize(len);
  partial_str.resize(len);
  ASSERT_EQ(AbstractHuffmanTable::CodeSymbol::HaveCommonPrefix(symbol, partial),
            symbol_str == partial_str)
      << "Where symbol_str = " << symbol_str
      << ", partial_str = " << partial_str;
}
TEST(CodeSymbolHaveCommonPrefixTest, BasicTest) {
  {
    // Self-check for common prefix equals true
    const AbstractHuffmanTable::CodeSymbol s(0b0, 1);
    ASSERT_TRUE(AbstractHuffmanTable::CodeSymbol::HaveCommonPrefix(s, s));
  }
  ASSERT_TRUE(
      AbstractHuffmanTable::CodeSymbol::HaveCommonPrefix({0b0, 1}, {0b0, 1}));
  ASSERT_TRUE(
      AbstractHuffmanTable::CodeSymbol::HaveCommonPrefix({0b10, 2}, {0b1, 1}));
  ASSERT_FALSE(
      AbstractHuffmanTable::CodeSymbol::HaveCommonPrefix({0b10, 2}, {0b0, 1}));
  ASSERT_FALSE(
      AbstractHuffmanTable::CodeSymbol::HaveCommonPrefix({0b10, 2}, {0b01, 2}));
}

#ifndef NDEBUG
TEST(CodeSymbolHaveCommonPrefixDeathTest, AsymmetricalDeathTest) {
  ASSERT_DEATH(
      {
        AbstractHuffmanTable::CodeSymbol::HaveCommonPrefix({0b0, 1}, {0b0, 2});
      },
      "partial.code_len <= symbol.code_len");
  ASSERT_DEATH(
      {
        AbstractHuffmanTable::CodeSymbol::HaveCommonPrefix({0b01, 2},
                                                           {0b010, 3});
      },
      "partial.code_len <= symbol.code_len");
}
#endif

auto genHT = [](std::initializer_list<uint8_t>&& nCodesPerLength)
    -> AbstractHuffmanTable {
  AbstractHuffmanTable ht;
  std::vector<uint8_t> v(nCodesPerLength.begin(), nCodesPerLength.end());
  v.resize(16);
  Buffer b(v.data(), v.size());
  ht.setNCodesPerLength(b);

  return ht;
};

auto genHTCount =
    [](std::initializer_list<uint8_t>&& nCodesPerLength) -> uint32_t {
  AbstractHuffmanTable ht;
  std::vector<uint8_t> v(nCodesPerLength.begin(), nCodesPerLength.end());
  v.resize(16);
  Buffer b(v.data(), v.size());
  return ht.setNCodesPerLength(b);
};

auto genHTFull =
    [](std::initializer_list<uint8_t>&& nCodesPerLength,
       std::initializer_list<uint8_t>&& codeValues) -> AbstractHuffmanTable {
  auto ht = genHT(std::move(nCodesPerLength));
  std::vector<uint8_t> v(codeValues.begin(), codeValues.end());
  Buffer b(v.data(), v.size());
  ht.setCodeValues(b);
  return ht;
};

#ifndef NDEBUG
TEST(AbstractHuffmanTableDeathTest, setNCodesPerLengthRequires16Lengths) {
  for (int i = 0; i < 32; i++) {
    std::vector<uint8_t> v(i, 1);
    ASSERT_EQ(v.size(), i);

    Buffer b(v.data(), v.size());
    ASSERT_EQ(b.getSize(), v.size());

    AbstractHuffmanTable ht;

    if (b.getSize() != 16) {
      ASSERT_DEATH({ ht.setNCodesPerLength(b); }, "data.getSize\\(\\) == 16");
    } else {
      ASSERT_EXIT(
          {
            ht.setNCodesPerLength(b);

            exit(0);
          },
          ::testing::ExitedWithCode(0), "");
    }
  }
}
#endif

TEST(AbstractHuffmanTableTest, setNCodesPerLengthEqualCompareAndTrimming) {
  {
    AbstractHuffmanTable a;
    AbstractHuffmanTable b;

    ASSERT_EQ(a, b);
  }

  ASSERT_EQ(genHT({1}), genHT({1}));
  ASSERT_EQ(genHT({1}), genHT({1, 0}));
  ASSERT_EQ(genHT({1, 0}), genHT({1}));
  ASSERT_EQ(genHT({1, 0}), genHT({1, 0}));
  ASSERT_EQ(genHT({0, 1}), genHT({0, 1}));
  ASSERT_EQ(genHT({1, 1}), genHT({1, 1}));

  ASSERT_NE(genHT({1, 0}), genHT({1, 1}));
  ASSERT_NE(genHT({0, 1}), genHT({1}));
  ASSERT_NE(genHT({0, 1}), genHT({1, 0}));
  ASSERT_NE(genHT({0, 1}), genHT({1, 1}));
  ASSERT_NE(genHT({1}), genHT({1, 1}));
}

TEST(AbstractHuffmanTableTest, setNCodesPerLengthEmptyIsBad) {
  ASSERT_THROW(genHT({}), rawspeed::RawDecoderException);
  ASSERT_THROW(genHT({0}), rawspeed::RawDecoderException);
  ASSERT_THROW(genHT({0, 0}), rawspeed::RawDecoderException);
}

TEST(AbstractHuffmanTableTest, setNCodesPerLengthTooManyCodesTotal) {
  ASSERT_NO_THROW(genHT({0, 0, 0, 0, 0, 0, 0, 162}));
  ASSERT_THROW(genHT({0, 0, 0, 0, 0, 0, 0, 163}),
               rawspeed::RawDecoderException);
}

TEST(AbstractHuffmanTableTest, setNCodesPerLengthTooManyCodesForLength) {
  for (int len = 1; len < 8; len++) {
    AbstractHuffmanTable ht;
    std::vector<uint8_t> v(16, 0);
    Buffer b(v.data(), v.size());
    for (auto i = 1U; i <= (1U << len); i++) {
      v[len - 1] = i;
      ASSERT_NO_THROW(ht.setNCodesPerLength(b););
    }
    v[len - 1]++;
    ASSERT_THROW(ht.setNCodesPerLength(b), rawspeed::RawDecoderException);
  }
}

TEST(AbstractHuffmanTableTest, setNCodesPerLengthCodeSymbolOverflow) {
  ASSERT_NO_THROW(genHT({1}));
  ASSERT_NO_THROW(genHT({2}));
  ASSERT_THROW(genHT({3}), rawspeed::RawDecoderException);
  ASSERT_NO_THROW(genHT({1, 2}));
  ASSERT_THROW(genHT({1, 3}), rawspeed::RawDecoderException);
  ASSERT_THROW(genHT({2, 1}), rawspeed::RawDecoderException);
  ASSERT_NO_THROW(genHT({0, 4}));
  ASSERT_THROW(genHT({0, 5}), rawspeed::RawDecoderException);
}

TEST(AbstractHuffmanTableTest, setNCodesPerLengthCounts) {
  ASSERT_EQ(genHTCount({1}), 1);
  ASSERT_EQ(genHTCount({1, 0}), 1);
  ASSERT_EQ(genHTCount({0, 1}), 1);
  ASSERT_EQ(genHTCount({0, 2}), 2);
  ASSERT_EQ(genHTCount({0, 3}), 3);
  ASSERT_EQ(genHTCount({1, 1}), 2);
  ASSERT_EQ(genHTCount({1, 2}), 3);
}

#ifndef NDEBUG
TEST(AbstractHuffmanTableDeathTest, setCodeValuesRequiresCount) {
  for (int len = 1; len < 8; len++) {
    AbstractHuffmanTable ht;
    std::vector<uint8_t> l(16, 0);
    Buffer bl(l.data(), l.size());
    l[len - 1] = (1U << len) - 1U;
    const auto count = ht.setNCodesPerLength(bl);
    std::vector<uint8_t> v;
    v.reserve(count + 1);
    for (auto cnt = count - 1; cnt <= count + 1; cnt++) {
      v.resize(cnt);
      Buffer bv(v.data(), v.size());
      if (cnt != count) {
        ASSERT_DEATH({ ht.setCodeValues(bv); },
                     "data.getSize\\(\\) == maxCodesCount\\(\\)");
      } else {
        ASSERT_EXIT(
            {
              ht.setCodeValues(bv);
              exit(0);
            },
            ::testing::ExitedWithCode(0), "");
      }
    }
  }
}

TEST(AbstractHuffmanTableDeathTest, setCodeValuesRequiresLessThan162) {
  auto ht = genHT({0, 0, 0, 0, 0, 0, 0, 162});
  std::vector<uint8_t> v(163, 0);
  Buffer bv(v.data(), v.size());
  ASSERT_DEATH({ ht.setCodeValues(bv); }, "data.getSize\\(\\) <= 162");
}
#endif

TEST(AbstractHuffmanTableTest, setCodeValuesValueLessThan16) {
  auto ht = genHT({1});
  std::vector<uint8_t> v(1);

  for (int i = 0; i < 256; i++) {
    v[0] = i;
    Buffer b(v.data(), v.size());
    ASSERT_NO_THROW(ht.setCodeValues(b););
  }
}

TEST(AbstractHuffmanTableTest, EqualCompareAndTrimming) {
  ASSERT_EQ(genHTFull({1}, {0}), genHTFull({1}, {0}));
  ASSERT_EQ(genHTFull({1}, {1}), genHTFull({1}, {1}));

  ASSERT_EQ(genHTFull({1}, {0}), genHTFull({1, 0}, {0}));
  ASSERT_EQ(genHTFull({1, 0}, {0}), genHTFull({1, 0}, {0}));
  ASSERT_EQ(genHTFull({1, 0}, {0}), genHTFull({1}, {0}));

  ASSERT_NE(genHTFull({1}, {0}), genHTFull({1}, {1}));
  ASSERT_NE(genHTFull({1}, {1}), genHTFull({1}, {0}));

  ASSERT_NE(genHTFull({1}, {0}), genHTFull({1, 0}, {1}));
  ASSERT_NE(genHTFull({1, 0}, {0}), genHTFull({1, 0}, {1}));
  ASSERT_NE(genHTFull({1, 0}, {0}), genHTFull({1}, {1}));
}

using SignExtendDataType = std::tuple<uint32_t, uint32_t, int>;
class SignExtendTest : public ::testing::TestWithParam<SignExtendDataType> {
protected:
  SignExtendTest() = default;
  virtual void SetUp() {
    auto p = GetParam();

    diff = std::get<0>(p);
    len = std::get<1>(p);
    value = std::get<2>(p);
  }

  uint32_t diff;
  uint32_t len;
  int value;
};

auto zeroDiff = [](int len) { return make_tuple(0, len, -((1 << len) - 1)); };
auto passthrough = [](int len) {
  return make_tuple(((1 << len) - 1), len, ((1 << len) - 1));
};
auto one = [](int len) { return make_tuple((1 << len), len, 1); };
static const SignExtendDataType signExtendData[]{
    // clang-format off
    zeroDiff(1),
    zeroDiff(2),
    zeroDiff(3),
    zeroDiff(4),
    zeroDiff(5),
    zeroDiff(6),
    zeroDiff(7),
    zeroDiff(8),
    zeroDiff(9),
    zeroDiff(10),
    zeroDiff(11),
    zeroDiff(12),
    zeroDiff(13),
    zeroDiff(14),
    zeroDiff(15),
    zeroDiff(16),

    passthrough(1),
    passthrough(2),
    passthrough(3),
    passthrough(4),
    passthrough(5),
    passthrough(6),
    passthrough(7),
    passthrough(8),
    passthrough(9),
    passthrough(10),
    passthrough(11),
    passthrough(12),
    passthrough(13),
    passthrough(14),
    passthrough(15),
    passthrough(16),

    one(1),
    one(2),
    one(3),
    one(4),
    one(5),
    one(6),
    one(7),
    one(8),
    one(9),
    one(10),
    one(11),
    one(12),
    one(13),
    one(14),
    one(15),
    one(16),

    make_tuple(0b00, 0b01, -0b001),
    make_tuple(0b01, 0b01,  0b001),
    make_tuple(0b10, 0b01,  0b001),
    make_tuple(0b11, 0b01,  0b011),
    make_tuple(0b00, 0b10, -0b011),
    make_tuple(0b01, 0b10, -0b010),
    make_tuple(0b10, 0b10,  0b010),
    make_tuple(0b11, 0b10,  0b011),
    make_tuple(0b00, 0b11, -0b111),
    make_tuple(0b01, 0b11, -0b110),
    make_tuple(0b10, 0b11, -0b101),
    make_tuple(0b11, 0b11, -0b100),
    // clang-format on
};
INSTANTIATE_TEST_CASE_P(SignExtendTest, SignExtendTest,
                        ::testing::ValuesIn(signExtendData));
TEST_P(SignExtendTest, SignExtendTest) {
  ASSERT_EQ(AbstractHuffmanTable::extend(diff, len), value);
}

using generateCodeSymbolsDataType =
    std::tuple<std::vector<uint8_t>,
               std::vector<AbstractHuffmanTable::CodeSymbol>>;
class generateCodeSymbolsTest
    : protected AbstractHuffmanTable,
      public ::testing::TestWithParam<generateCodeSymbolsDataType> {
protected:
  generateCodeSymbolsTest() = default;
  virtual void SetUp() {
    auto p = GetParam();

    ncpl = std::get<0>(p);
    ncpl.resize(16);
    expectedSymbols = std::get<1>(p);
  }

  std::vector<uint8_t> ncpl;
  std::vector<AbstractHuffmanTable::CodeSymbol> expectedSymbols;
};
static const generateCodeSymbolsDataType generateCodeSymbolsData[]{
    make_tuple(std::vector<uint8_t>{1},
               std::vector<AbstractHuffmanTable::CodeSymbol>{{0b0, 1}}),

    make_tuple(std::vector<uint8_t>{0, 1},
               std::vector<AbstractHuffmanTable::CodeSymbol>{
                   {0b00, 2},
               }),
    make_tuple(std::vector<uint8_t>{0, 2},
               std::vector<AbstractHuffmanTable::CodeSymbol>{
                   {0b00, 2},
                   {0b01, 2},
               }),
    make_tuple(std::vector<uint8_t>{0, 3},
               std::vector<AbstractHuffmanTable::CodeSymbol>{
                   {0b00, 2},
                   {0b01, 2},
                   {0b10, 2},
               }),

    make_tuple(std::vector<uint8_t>{1, 1},
               std::vector<AbstractHuffmanTable::CodeSymbol>{
                   {0b0, 1},
                   {0b10, 2},
               }),
    make_tuple(std::vector<uint8_t>{1, 2},
               std::vector<AbstractHuffmanTable::CodeSymbol>{
                   {0b0, 1},
                   {0b10, 2},
                   {0b11, 2},
               }),

};
INSTANTIATE_TEST_CASE_P(generateCodeSymbolsTest, generateCodeSymbolsTest,
                        ::testing::ValuesIn(generateCodeSymbolsData));
TEST_P(generateCodeSymbolsTest, generateCodeSymbolsTest) {
  Buffer bl(ncpl.data(), ncpl.size());
  const auto cnt = setNCodesPerLength(bl);
  std::vector<uint8_t> cv(cnt, 0);
  Buffer bv(cv.data(), cv.size());
  setCodeValues(bv);

  ASSERT_EQ(generateCodeSymbols(), expectedSymbols);
}

class DummyHuffmanTableTest : public AbstractHuffmanTable,
                              public ::testing::Test {};
using DummyHuffmanTableDeathTest = DummyHuffmanTableTest;

#ifndef NDEBUG
TEST_F(DummyHuffmanTableDeathTest, VerifyCodeSymbolsTest) {
  {
    std::vector<AbstractHuffmanTable::CodeSymbol> s{{0b0, 1}};
    ASSERT_EXIT(
        {
          VerifyCodeSymbols(s);

          exit(0);
        },
        ::testing::ExitedWithCode(0), "");
  }
  {
    // Duplicates are not ok.
    std::vector<AbstractHuffmanTable::CodeSymbol> s{{0b0, 1}, {0b0, 1}};
    ASSERT_DEATH({ VerifyCodeSymbols(s); },
                 "all code symbols are globally ordered");
  }
  {
    std::vector<AbstractHuffmanTable::CodeSymbol> s{{0b0, 1}, {0b1, 1}};
    ASSERT_EXIT(
        {
          VerifyCodeSymbols(s);

          exit(0);
        },
        ::testing::ExitedWithCode(0), "");
  }
  {
    // Code Symbols are strictly increasing
    std::vector<AbstractHuffmanTable::CodeSymbol> s{{0b1, 1}, {0b0, 1}};
    ASSERT_DEATH({ VerifyCodeSymbols(s); },
                 "all code symbols are globally ordered");
  }
  {
    // Code Lengths are not decreasing
    std::vector<AbstractHuffmanTable::CodeSymbol> s{{0b0, 2}, {0b1, 1}};
    ASSERT_DEATH({ VerifyCodeSymbols(s); },
                 "all code symbols are globally ordered");
  }
  {
    // Reverse order
    std::vector<AbstractHuffmanTable::CodeSymbol> s{{0b10, 2}, {0b0, 1}};
    ASSERT_DEATH({ VerifyCodeSymbols(s); },
                 "all code symbols are globally ordered");
  }
  {
    // Can not have common prefixes
    std::vector<AbstractHuffmanTable::CodeSymbol> s{{0b0, 1}, {0b01, 2}};
    ASSERT_DEATH({ VerifyCodeSymbols(s); }, "!CodeSymbol::HaveCommonPrefix");
  }
}
#endif

} // namespace rawspeed_test
