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

#include <cstdint> // for uint32_t, uint8_t, UINT32_C
#include <cstdio>  // for printf, snprintf
#include <cstring> // for memset, strlen, memcmp, memcpy
#include <string>  // for string

#ifdef ENABLE_SELFTEST
#include <cstdlib> // for EXIT_FAILURE, EXIT_SUCCESS
#endif

void md5_compress(uint32_t state[4], const uint8_t block[64]) {
#define LOADSCHEDULE(i)                                                        \
  schedule[i] =                                                                \
      (uint32_t)block[(i)*4 + 0] << 0 | (uint32_t)block[(i)*4 + 1] << 8 |      \
      (uint32_t)block[(i)*4 + 2] << 16 | (uint32_t)block[(i)*4 + 3] << 24;

  uint32_t schedule[16];
  LOADSCHEDULE(0)
  LOADSCHEDULE(1)
  LOADSCHEDULE(2)
  LOADSCHEDULE(3)
  LOADSCHEDULE(4)
  LOADSCHEDULE(5)
  LOADSCHEDULE(6)
  LOADSCHEDULE(7)
  LOADSCHEDULE(8)
  LOADSCHEDULE(9)
  LOADSCHEDULE(10)
  LOADSCHEDULE(11)
  LOADSCHEDULE(12)
  LOADSCHEDULE(13)
  LOADSCHEDULE(14)
  LOADSCHEDULE(15)

#define ROTL32(x, n)                                                           \
  (((0U + (x)) << (n)) |                                                       \
   ((x) >> (32 - (n)))) // Assumes that x is uint32_t and 0 < n < 32
#define ROUND0(a, b, c, d, k, s, t)                                            \
  ROUND_TAIL(a, b, (d) ^ ((b) & ((c) ^ (d))), k, s, t)
#define ROUND1(a, b, c, d, k, s, t)                                            \
  ROUND_TAIL(a, b, (c) ^ ((d) & ((b) ^ (c))), k, s, t)
#define ROUND2(a, b, c, d, k, s, t) ROUND_TAIL(a, b, (b) ^ (c) ^ (d), k, s, t)
#define ROUND3(a, b, c, d, k, s, t)                                            \
  ROUND_TAIL(a, b, (c) ^ ((b) | ~(d)), k, s, t)
#define ROUND_TAIL(a, b, expr, k, s, t)                                        \
  a = 0U + (a) + (expr) + UINT32_C(t) + schedule[k];                           \
  (a) = 0U + (b) + ROTL32(a, s);

  uint32_t a = state[0];
  uint32_t b = state[1];
  uint32_t c = state[2];
  uint32_t d = state[3];

  ROUND0(a, b, c, d, 0, 7, 0xD76AA478)
  ROUND0(d, a, b, c, 1, 12, 0xE8C7B756)
  ROUND0(c, d, a, b, 2, 17, 0x242070DB)
  ROUND0(b, c, d, a, 3, 22, 0xC1BDCEEE)
  ROUND0(a, b, c, d, 4, 7, 0xF57C0FAF)
  ROUND0(d, a, b, c, 5, 12, 0x4787C62A)
  ROUND0(c, d, a, b, 6, 17, 0xA8304613)
  ROUND0(b, c, d, a, 7, 22, 0xFD469501)
  ROUND0(a, b, c, d, 8, 7, 0x698098D8)
  ROUND0(d, a, b, c, 9, 12, 0x8B44F7AF)
  ROUND0(c, d, a, b, 10, 17, 0xFFFF5BB1)
  ROUND0(b, c, d, a, 11, 22, 0x895CD7BE)
  ROUND0(a, b, c, d, 12, 7, 0x6B901122)
  ROUND0(d, a, b, c, 13, 12, 0xFD987193)
  ROUND0(c, d, a, b, 14, 17, 0xA679438E)
  ROUND0(b, c, d, a, 15, 22, 0x49B40821)
  ROUND1(a, b, c, d, 1, 5, 0xF61E2562)
  ROUND1(d, a, b, c, 6, 9, 0xC040B340)
  ROUND1(c, d, a, b, 11, 14, 0x265E5A51)
  ROUND1(b, c, d, a, 0, 20, 0xE9B6C7AA)
  ROUND1(a, b, c, d, 5, 5, 0xD62F105D)
  ROUND1(d, a, b, c, 10, 9, 0x02441453)
  ROUND1(c, d, a, b, 15, 14, 0xD8A1E681)
  ROUND1(b, c, d, a, 4, 20, 0xE7D3FBC8)
  ROUND1(a, b, c, d, 9, 5, 0x21E1CDE6)
  ROUND1(d, a, b, c, 14, 9, 0xC33707D6)
  ROUND1(c, d, a, b, 3, 14, 0xF4D50D87)
  ROUND1(b, c, d, a, 8, 20, 0x455A14ED)
  ROUND1(a, b, c, d, 13, 5, 0xA9E3E905)
  ROUND1(d, a, b, c, 2, 9, 0xFCEFA3F8)
  ROUND1(c, d, a, b, 7, 14, 0x676F02D9)
  ROUND1(b, c, d, a, 12, 20, 0x8D2A4C8A)
  ROUND2(a, b, c, d, 5, 4, 0xFFFA3942)
  ROUND2(d, a, b, c, 8, 11, 0x8771F681)
  ROUND2(c, d, a, b, 11, 16, 0x6D9D6122)
  ROUND2(b, c, d, a, 14, 23, 0xFDE5380C)
  ROUND2(a, b, c, d, 1, 4, 0xA4BEEA44)
  ROUND2(d, a, b, c, 4, 11, 0x4BDECFA9)
  ROUND2(c, d, a, b, 7, 16, 0xF6BB4B60)
  ROUND2(b, c, d, a, 10, 23, 0xBEBFBC70)
  ROUND2(a, b, c, d, 13, 4, 0x289B7EC6)
  ROUND2(d, a, b, c, 0, 11, 0xEAA127FA)
  ROUND2(c, d, a, b, 3, 16, 0xD4EF3085)
  ROUND2(b, c, d, a, 6, 23, 0x04881D05)
  ROUND2(a, b, c, d, 9, 4, 0xD9D4D039)
  ROUND2(d, a, b, c, 12, 11, 0xE6DB99E5)
  ROUND2(c, d, a, b, 15, 16, 0x1FA27CF8)
  ROUND2(b, c, d, a, 2, 23, 0xC4AC5665)
  ROUND3(a, b, c, d, 0, 6, 0xF4292244)
  ROUND3(d, a, b, c, 7, 10, 0x432AFF97)
  ROUND3(c, d, a, b, 14, 15, 0xAB9423A7)
  ROUND3(b, c, d, a, 5, 21, 0xFC93A039)
  ROUND3(a, b, c, d, 12, 6, 0x655B59C3)
  ROUND3(d, a, b, c, 3, 10, 0x8F0CCC92)
  ROUND3(c, d, a, b, 10, 15, 0xFFEFF47D)
  ROUND3(b, c, d, a, 1, 21, 0x85845DD1)
  ROUND3(a, b, c, d, 8, 6, 0x6FA87E4F)
  ROUND3(d, a, b, c, 15, 10, 0xFE2CE6E0)
  ROUND3(c, d, a, b, 6, 15, 0xA3014314)
  ROUND3(b, c, d, a, 13, 21, 0x4E0811A1)
  ROUND3(a, b, c, d, 4, 6, 0xF7537E82)
  ROUND3(d, a, b, c, 11, 10, 0xBD3AF235)
  ROUND3(c, d, a, b, 2, 15, 0x2AD7D2BB)
  ROUND3(b, c, d, a, 9, 21, 0xEB86D391)

  state[0] = 0U + state[0] + a;
  state[1] = 0U + state[1] + b;
  state[2] = 0U + state[2] + c;
  state[3] = 0U + state[3] + d;
}

/* Full message hasher */

void md5_hash(const uint8_t *message, size_t len, uint32_t hash[4]) {
  hash[0] = UINT32_C(0x67452301);
  hash[1] = UINT32_C(0xEFCDAB89);
  hash[2] = UINT32_C(0x98BADCFE);
  hash[3] = UINT32_C(0x10325476);

  size_t i;
  for (i = 0; len - i >= 64; i += 64)
    md5_compress(hash, &message[i]);

  uint8_t block[64];
  size_t rem = len - i;
  memcpy(block, &message[i], rem);

  block[rem] = 0x80;
  rem++;
  if (64 - rem >= 8)
    memset(&block[rem], 0, 56 - rem);
  else {
    memset(&block[rem], 0, 64 - rem);
    md5_compress(hash, block);
    memset(block, 0, 56);
  }

  block[64 - 8] = (uint8_t)((len & 0x1FU) << 3);
  len >>= 5;
  for (i = 1; i < 8; i++, len >>= 8)
    block[64 - 8 + i] = (uint8_t)len;
  md5_compress(hash, block);
}

std::string md5_hash(const uint8_t *message, size_t len) {
  uint32_t hash[4];
  md5_hash(message, len, hash);
  char res[2 * sizeof(hash) + 1];
  auto *h = (uint8_t *)hash;
  for (int i = 0; i < (int)sizeof(hash); ++i)
    snprintf(res + 2 * i, 3, "%02x", h[i]);
  res[32] = 0;
  return res;
}

#ifdef ENABLE_SELFTEST
/* Self-check */

struct testcase {
	uint32_t answer[4];
	const uint8_t *message;
};

#define TESTCASE(a, b, c, d, msg)                                              \
  { {UINT32_C(a), UINT32_C(b), UINT32_C(c), UINT32_C(d)}, (const uint8_t *)(msg) }

// Note: The MD5 standard specifies that uint32 are serialized to/from bytes in little endian
static struct testcase testCases[] = {
	TESTCASE(0xD98C1DD4,0x04B2008F,0x980980E9,0x7E42F8EC, ""),
	TESTCASE(0xB975C10C,0xA8B6F1C0,0xE299C331,0x61267769, "a"),
	TESTCASE(0x98500190,0xB04FD23C,0x7D3F96D6,0x727FE128, "abc"),
	TESTCASE(0x7D696BF9,0x8D93B77C,0x312F5A52,0xD061F1AA, "message digest"),
	TESTCASE(0xD7D3FCC3,0x00E49261,0x6C49FB7D,0x3BE167CA, "abcdefghijklmnopqrstuvwxyz"),
	TESTCASE(0x98AB74D1,0xF5D977D2,0x2C1C61A5,0x9F9D419F, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"),
	TESTCASE(0xA2F4ED57,0x55C9E32B,0x2EDA49AC,0x7AB60721, "12345678901234567890123456789012345678901234567890123456789012345678901234567890"),
};

static bool self_check() {
	unsigned int i;
	for (i = 0; i < sizeof(testCases) / sizeof(testCases[i]); i++) {
		struct testcase *tc = &testCases[i];
		uint32_t hash[4];
		md5_hash(tc->message, strlen((const char *)tc->message), hash);
		printf("%s -> %s\n", tc->message, md5_hash(tc->message, strlen((const char *)tc->message)).c_str());
		if (memcmp(hash, tc->answer, sizeof(tc->answer)) != 0)
			return false;
	}
	return true;
}

/* Main program */

int main(int argc, char **argv) {
	if (!self_check()) {
		printf("Self-check failed\n");
		return EXIT_FAILURE;
	}
	printf("Self-check passed\n");

#if 0
	// Benchmark speed
	uint32_t state[4] = {0};
	uint8_t block[64] = {0};
	const int N = 10000000;
	clock_t start_time = clock();
	int i;
	for (i = 0; i < N; i++)
		md5_compress(state, block);
	printf("Speed: %.1f MB/s\n", (double)N * sizeof(block) / (clock() - start_time) * CLOCKS_PER_SEC / 1000000);
#endif

	return EXIT_SUCCESS;
}
#endif
