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

#include "md5.h"
#include <cstdint> // for uint32_t, uint8_t
#include <cstdio>  // for size_t, snprintf
#include <cstring> // for memset, memcpy
#include <string>  // for allocator, string

namespace rawspeed::md5 {

// hashes 64 bytes at once
static void md5_compress(md5_state* state, const uint8_t* block);

static void md5_compress(md5_state* state, const uint8_t* block) {
  std::array<uint32_t, 16> schedule = {{}};

  auto LOADSCHEDULE = [&block, &schedule](int i) {
    for (int k = 3; k >= 0; k--)
      schedule[i] |= uint32_t(block[4 * i + k]) << (8 * k);
  };

  for (int i = 0; i < 16; i++)
    LOADSCHEDULE(i);

  // Assumes that x is uint32_t and 0 < n < 32
  auto ROTL32 = [](uint32_t x, int n) __attribute__((pure)) {
    return (x << n) | (x >> (32 - n));
  };

  auto ROUND_TAIL = [ROTL32, &schedule](uint32_t& a, uint32_t b, uint32_t expr,
                                        uint32_t k, uint32_t s, uint32_t t) {
    a = uint32_t(0UL + a + expr + t + schedule[k]);
    a = uint32_t(0UL + b + ROTL32(a, s));
  };

  auto ROUND0 = [ROUND_TAIL](uint32_t& a, uint32_t b, uint32_t c, uint32_t d,
                             uint32_t k, uint32_t s, uint32_t t) {
    ROUND_TAIL(a, b, d ^ (b & (c ^ d)), k, s, t);
  };

  auto ROUND1 = [ROUND_TAIL](uint32_t& a, uint32_t b, uint32_t c, uint32_t d,
                             uint32_t k, uint32_t s, uint32_t t) {
    ROUND_TAIL(a, b, c ^ (d & (b ^ c)), k, s, t);
  };

  auto ROUND2 = [ROUND_TAIL](uint32_t& a, uint32_t b, uint32_t c, uint32_t d,
                             uint32_t k, uint32_t s, uint32_t t) {
    ROUND_TAIL(a, b, b ^ c ^ d, k, s, t);
  };

  auto ROUND3 = [ROUND_TAIL](uint32_t& a, uint32_t b, uint32_t c, uint32_t d,
                             uint32_t k, uint32_t s, uint32_t t) {
    ROUND_TAIL(a, b, c ^ (b | ~d), k, s, t);
  };

  uint32_t a = (*state)[0];
  uint32_t b = (*state)[1];
  uint32_t c = (*state)[2];
  uint32_t d = (*state)[3];

  ROUND0(a, b, c, d, 0, 7, 0xD76AA478);
  ROUND0(d, a, b, c, 1, 12, 0xE8C7B756);
  ROUND0(c, d, a, b, 2, 17, 0x242070DB);
  ROUND0(b, c, d, a, 3, 22, 0xC1BDCEEE);
  ROUND0(a, b, c, d, 4, 7, 0xF57C0FAF);
  ROUND0(d, a, b, c, 5, 12, 0x4787C62A);
  ROUND0(c, d, a, b, 6, 17, 0xA8304613);
  ROUND0(b, c, d, a, 7, 22, 0xFD469501);
  ROUND0(a, b, c, d, 8, 7, 0x698098D8);
  ROUND0(d, a, b, c, 9, 12, 0x8B44F7AF);
  ROUND0(c, d, a, b, 10, 17, 0xFFFF5BB1);
  ROUND0(b, c, d, a, 11, 22, 0x895CD7BE);
  ROUND0(a, b, c, d, 12, 7, 0x6B901122);
  ROUND0(d, a, b, c, 13, 12, 0xFD987193);
  ROUND0(c, d, a, b, 14, 17, 0xA679438E);
  ROUND0(b, c, d, a, 15, 22, 0x49B40821);
  ROUND1(a, b, c, d, 1, 5, 0xF61E2562);
  ROUND1(d, a, b, c, 6, 9, 0xC040B340);
  ROUND1(c, d, a, b, 11, 14, 0x265E5A51);
  ROUND1(b, c, d, a, 0, 20, 0xE9B6C7AA);
  ROUND1(a, b, c, d, 5, 5, 0xD62F105D);
  ROUND1(d, a, b, c, 10, 9, 0x02441453);
  ROUND1(c, d, a, b, 15, 14, 0xD8A1E681);
  ROUND1(b, c, d, a, 4, 20, 0xE7D3FBC8);
  ROUND1(a, b, c, d, 9, 5, 0x21E1CDE6);
  ROUND1(d, a, b, c, 14, 9, 0xC33707D6);
  ROUND1(c, d, a, b, 3, 14, 0xF4D50D87);
  ROUND1(b, c, d, a, 8, 20, 0x455A14ED);
  ROUND1(a, b, c, d, 13, 5, 0xA9E3E905);
  ROUND1(d, a, b, c, 2, 9, 0xFCEFA3F8);
  ROUND1(c, d, a, b, 7, 14, 0x676F02D9);
  ROUND1(b, c, d, a, 12, 20, 0x8D2A4C8A);
  ROUND2(a, b, c, d, 5, 4, 0xFFFA3942);
  ROUND2(d, a, b, c, 8, 11, 0x8771F681);
  ROUND2(c, d, a, b, 11, 16, 0x6D9D6122);
  ROUND2(b, c, d, a, 14, 23, 0xFDE5380C);
  ROUND2(a, b, c, d, 1, 4, 0xA4BEEA44);
  ROUND2(d, a, b, c, 4, 11, 0x4BDECFA9);
  ROUND2(c, d, a, b, 7, 16, 0xF6BB4B60);
  ROUND2(b, c, d, a, 10, 23, 0xBEBFBC70);
  ROUND2(a, b, c, d, 13, 4, 0x289B7EC6);
  ROUND2(d, a, b, c, 0, 11, 0xEAA127FA);
  ROUND2(c, d, a, b, 3, 16, 0xD4EF3085);
  ROUND2(b, c, d, a, 6, 23, 0x04881D05);
  ROUND2(a, b, c, d, 9, 4, 0xD9D4D039);
  ROUND2(d, a, b, c, 12, 11, 0xE6DB99E5);
  ROUND2(c, d, a, b, 15, 16, 0x1FA27CF8);
  ROUND2(b, c, d, a, 2, 23, 0xC4AC5665);
  ROUND3(a, b, c, d, 0, 6, 0xF4292244);
  ROUND3(d, a, b, c, 7, 10, 0x432AFF97);
  ROUND3(c, d, a, b, 14, 15, 0xAB9423A7);
  ROUND3(b, c, d, a, 5, 21, 0xFC93A039);
  ROUND3(a, b, c, d, 12, 6, 0x655B59C3);
  ROUND3(d, a, b, c, 3, 10, 0x8F0CCC92);
  ROUND3(c, d, a, b, 10, 15, 0xFFEFF47D);
  ROUND3(b, c, d, a, 1, 21, 0x85845DD1);
  ROUND3(a, b, c, d, 8, 6, 0x6FA87E4F);
  ROUND3(d, a, b, c, 15, 10, 0xFE2CE6E0);
  ROUND3(c, d, a, b, 6, 15, 0xA3014314);
  ROUND3(b, c, d, a, 13, 21, 0x4E0811A1);
  ROUND3(a, b, c, d, 4, 6, 0xF7537E82);
  ROUND3(d, a, b, c, 11, 10, 0xBD3AF235);
  ROUND3(c, d, a, b, 2, 15, 0x2AD7D2BB);
  ROUND3(b, c, d, a, 9, 21, 0xEB86D391);

  (*state)[0] = uint32_t(0UL + (*state)[0] + a);
  (*state)[1] = uint32_t(0UL + (*state)[1] + b);
  (*state)[2] = uint32_t(0UL + (*state)[2] + c);
  (*state)[3] = uint32_t(0UL + (*state)[3] + d);
}

/* Full message hasher */

void md5_hash(const uint8_t* message, size_t len, md5_state* hash) {
  *hash = md5_init;

  size_t i;
  for (i = 0; len - i >= 64; i += 64)
    md5_compress(hash, &message[i]);

  std::array<uint8_t, 64> block;
  size_t rem = len - i;
  memcpy(block.data(), &message[i], rem);

  block[rem] = 0x80;
  rem++;
  if (64 - rem >= 8)
    memset(&block[rem], 0, 56 - rem);
  else {
    memset(&block[rem], 0, 64 - rem);
    md5_compress(hash, block.data());
    memset(block.data(), 0, 56);
  }

  block[64 - 8] = static_cast<uint8_t>((len & 0x1FU) << 3);
  len >>= 5;
  for (i = 1; i < 8; i++) {
    block[64 - 8 + i] = static_cast<uint8_t>(len);
    len >>= 8;
  }
  md5_compress(hash, block.data());
}

std::string hash_to_string(const md5_state& hash) {
  std::array<char, 2 * sizeof(hash) + 1> res;
  const auto* h = reinterpret_cast<const uint8_t*>(&hash[0]);
  for (int i = 0; i < static_cast<int>(sizeof(hash)); ++i)
    snprintf(&res[2 * i], 3, "%02x", h[i]);
  res[32] = 0;
  return res.data();
}

std::string md5_hash(const uint8_t* message, size_t len) {
  md5_state hash;
  md5_hash(message, len, &hash);
  return hash_to_string(hash);
}

} // namespace rawspeed::md5
