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

#include <array>   // for array
#include <cstdint> // for uint8_t, uint32_t
#include <cstdio>  // for size_t
#include <string>  // for string

namespace rawspeed {

namespace md5 {

using md5_state = std::array<uint32_t, 4>;

static constexpr const md5_state md5_init = {
    {UINT32_C(0x67452301), UINT32_C(0xEFCDAB89), UINT32_C(0x98BADCFE),
     UINT32_C(0x10325476)}};

// computes hash of the buffer message with length len
void md5_hash(const uint8_t* message, size_t len, md5_state& hash);

// returns hash as string
std::string hash_to_string(const md5_state& hash);

// computes hash of the buffer message with length len and returns it as string
std::string md5_hash(const uint8_t* message, size_t len);

} // namespace md5

} // namespace rawspeed
