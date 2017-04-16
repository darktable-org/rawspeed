/*
 * MD5 hash in C and x86 assembly
 *
 * Copyright (c) 2016 Project Nayuki
 * https://www.nayuki.io/page/fast-md5-hash-implementation-in-x86-assembly
 *
 * (MIT License)
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of
 * the Software, and to permit persons to whom the Software is furnished to do
 * so,
 * subject to the following conditions:
 * - The above copyright notice and this permission notice shall be included in
 *   all copies or substantial portions of the Software.
 * - The Software is provided "as is", without warranty of any kind, express or
 *   implied, including but not limited to the warranties of merchantability,
 *   fitness for a particular purpose and noninfringement. In no event shall the
 *   authors or copyright holders be liable for any claim, damages or other
 *   liability, whether in an action of contract, tort or otherwise, arising
 * from,
 *   out of or in connection with the Software or the use or other dealings in
 * the
 *   Software.
 */

#include "md5.h"         // for rawspeed::md5::state, md5_hash
#include <array>         // for array
#include <cstdint>       // for UINT32_C, uint8_t
#include <cstring>       // for strlen
#include <gtest/gtest.h> // for AssertionResult, IsNullLiteralHelper, Param...
#include <utility>       // for pair, make_pair

using MD5Testcase = std::pair<rawspeed::md5::md5_state, const uint8_t*>;
class MD5Test : public ::testing::TestWithParam<MD5Testcase> {
protected:
  MD5Test() = default;
  virtual void SetUp() override {
    auto p = GetParam();

    answer = p.first;
    message = p.second;
  }

  rawspeed::md5::md5_state answer;
  const uint8_t* message;
};

#define TESTCASE(a, b, c, d, msg)                                              \
  {                                                                            \
    std::make_pair((rawspeed::md5::md5_state){{UINT32_C(a), UINT32_C(b),       \
                                               UINT32_C(c), UINT32_C(d)}},     \
                   (const uint8_t*)(msg))                                      \
  }

// Note: The MD5 standard specifies that uint32 are serialized to/from bytes in
// little endian
static MD5Testcase testCases[] = {
    TESTCASE(0xD98C1DD4, 0x04B2008F, 0x980980E9, 0x7E42F8EC, ""),
    TESTCASE(0xB975C10C, 0xA8B6F1C0, 0xE299C331, 0x61267769, "a"),
    TESTCASE(0x98500190, 0xB04FD23C, 0x7D3F96D6, 0x727FE128, "abc"),
    TESTCASE(0x7D696BF9, 0x8D93B77C, 0x312F5A52, 0xD061F1AA, "message digest"),
    TESTCASE(0xD7D3FCC3, 0x00E49261, 0x6C49FB7D, 0x3BE167CA,
             "abcdefghijklmnopqrstuvwxyz"),
    TESTCASE(0x98AB74D1, 0xF5D977D2, 0x2C1C61A5, 0x9F9D419F,
             "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"),
    TESTCASE(0xA2F4ED57, 0x55C9E32B, 0x2EDA49AC, 0x7AB60721,
             "12345678901234567890123456789012345678901234567890123456789012345"
             "678901234567890"),
};

INSTANTIATE_TEST_CASE_P(MD5Test, MD5Test, ::testing::ValuesIn(testCases));
TEST_P(MD5Test, CheckTestCaseSet) {
  ASSERT_NO_THROW({
    rawspeed::md5::md5_state hash;
    rawspeed::md5::md5_hash(message, strlen((const char*)message), &hash);

    ASSERT_EQ(hash, answer);
  });
}
