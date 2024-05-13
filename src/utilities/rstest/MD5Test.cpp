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
#include "adt/Casts.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <tuple>
#include <utility>
#include <gtest/gtest.h>

using MD5Testcase =
    std::pair<rawspeed::md5::MD5Hasher::state_type, const uint8_t*>;
class MD5Test : public ::testing::TestWithParam<MD5Testcase> {
  virtual void anchor() const;

public:
  MD5Test() = default;
  void SetUp() final { std::tie(answer, message) = GetParam(); }

  rawspeed::md5::MD5Hasher::state_type answer;
  const uint8_t* message = nullptr;
};

void MD5Test::anchor() const {
  // Empty out-of-line definition for the purpose of anchoring
  // the class's vtable to this Translational Unit.
}

constexpr auto TESTCASE = [](uint32_t a, uint32_t b, uint32_t c, uint32_t d,
                             const char* msg) -> MD5Testcase {
  return {{a, b, c, d}, reinterpret_cast<const uint8_t*>(msg)};
};

// Note: The MD5 standard specifies that uint32_t are serialized to/from bytes
// in little endian
static const std::array<MD5Testcase, 136> testCases = {
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
    TESTCASE(0xD98C1DD4, 0x04B2008F, 0x980980E9, 0x7E42F8EC, ""),
    TESTCASE(0xD4D3E1E1, 0x7E127305, 0x0C48E09E, 0xD68312AF, "R"),
    TESTCASE(0xF7E9FE75, 0xFE65DE95, 0x95A48741, 0x4A595107, "bF"),
    TESTCASE(0xD3B8EFDE, 0xEBD5F531, 0xC2D0C6ED, 0x09DDF0BD, "ktU"),
    TESTCASE(0xDA3BF4F7, 0xEB74322C, 0x02EBAF51, 0x9B68AC18, "G4sK"),
    TESTCASE(0xF220A8F7, 0x00AC8633, 0x5A90DA7F, 0x206606FF, "GzgRg"),
    TESTCASE(0x7A4EF06A, 0x84C285C7, 0x43832E73, 0xA8C84581, "mKMhh6"),
    TESTCASE(0xF7762493, 0x9AE6C1CE, 0x09E01F5A, 0x9CB777F7, "09hHanV"),
    TESTCASE(0xE4C7E1E5, 0xEB978CBD, 0xB773F1C6, 0x3496C42E, "wdKoKG7I"),
    TESTCASE(0xFB802A54, 0xCACFAAA2, 0x491FF9B7, 0xACF45BD0, "oFoWcnsWD"),
    TESTCASE(0xC00BD82E, 0x984BF1D4, 0x11026E97, 0x05AC18A6, "HZ10Bk8H5K"),
    TESTCASE(0xAD9EF3A6, 0x5AD42D41, 0xF3CF3CAB, 0x9047DD41, "VL3aZfIOcpk"),
    TESTCASE(0xD2935D44, 0x4F433935, 0xB1ACF14E, 0x9C627042, "smPeJPYeSmha"),
    TESTCASE(0xF8C1AA11, 0x46FDCE29, 0x6B80F6F7, 0x977B1692, "tv00GjvVbY2Og"),
    TESTCASE(0xA9A80D87, 0x938AD6B5, 0x45796A35, 0x9F533A6E, "Q8U2K4BCnCV11a"),
    TESTCASE(0xBFA8E8CF, 0x8113E1A6, 0x5FF6886A, 0xCF364304, "6aie78iUwSnk73P"),
    TESTCASE(0x07A0DCC7, 0x6803F0D2, 0x6F101FCD, 0xD1AADC57,
             "XuIrsMeJvzsCO7zB"),
    TESTCASE(0x57E771F0, 0xCCF3BFA9, 0x348CA8DA, 0xD701ED1C,
             "7i7E7MfpNBVrlSIfF"),
    TESTCASE(0x8FDB84A2, 0xAE2F1808, 0x411CC797, 0x2B04779F,
             "c34xKvGQIIWCZNhAj8"),
    TESTCASE(0xEE3F232D, 0x22EA3BBE, 0xE106DC07, 0x47C41C8F,
             "tF6FU0hqDcQ2YKyhYrG"),
    TESTCASE(0x8F83091A, 0xF9E3E1E7, 0xF5190C94, 0xC1207239,
             "8WeBcM6HvwNNiNuBG1pZ"),
    TESTCASE(0x256C3C27, 0x8EA4A4D3, 0xC7D18A81, 0xF5DED58C,
             "HgYf7DBEBefrsRcIed5hP"),
    TESTCASE(0xEC405417, 0x4A3E02A1, 0x01523576, 0xFFF7BB5A,
             "PDvff6QNerDHxjAn3W7I76"),
    TESTCASE(0x16D58F31, 0xA56393EE, 0x14D86BD3, 0xC22EAD07,
             "SGJQvtWfqPhpZQjehTHTH91"),
    TESTCASE(0xEFED9C77, 0xA94F5B69, 0x391FF963, 0xD20BA203,
             "H3yAcpmZrAQcbzbt6EyoI3aX"),
    TESTCASE(0x221B0B77, 0xD1DD32BD, 0x2CE8C75C, 0x5AA8F276,
             "OaQJ9pLKK1LR7UJlslMSpCclP"),
    TESTCASE(0x69EE970C, 0xE5A3AFDE, 0xDED4AB82, 0xD37EC9F4,
             "gG7Ug4ukuKUeAIrL4TXwuSLrit"),
    TESTCASE(0x63EDAA8D, 0x50F9DE56, 0x40733E0E, 0x8954F890,
             "k3ejOY5hbDaptsZ29HM2wNe1Ax0"),
    TESTCASE(0x66ED7D7D, 0x5B05CD3F, 0x5325ECEA, 0x3D8D4A81,
             "fW1Xk48tbXTelPbhfqcVhZRQ8Tyf"),
    TESTCASE(0x0EB3C186, 0x7E417CF3, 0x3C169DC0, 0x0E650915,
             "WmZzClNzZRfLWlEjyjJPgJ6QAMmBN"),
    TESTCASE(0x32E15253, 0x915D0B52, 0x609DA449, 0xFD26B521,
             "QqzlDhECVECa00qJ6cYy7ce1NARfw2"),
    TESTCASE(0x4DDC9302, 0xF831D885, 0x9DC5E249, 0xF988B99B,
             "y6Gfm3gpZIAvbLUbBi9Z5cBbe9NZSRI"),
    TESTCASE(0xD05B2026, 0xF58667B3, 0x90986570, 0xE06A1235,
             "WkIMzKROmGtzZTdAxVuKbPuEhH9H9Tr3"),
    TESTCASE(0xC2D5EE46, 0xBA74921F, 0xE6807A7D, 0xE8C57F7C,
             "XUmfmOc1pA0wxzdQxPZaxz1adAYcP4xVy"),
    TESTCASE(0xD693E9E0, 0x3AB2CB26, 0x9C4996E0, 0x7A005297,
             "bpXjD19JKq4hR7uLr5x6qtbaOdKVAV3q8S"),
    TESTCASE(0x0446A62E, 0xB1FF2DF6, 0xF0C1DBC7, 0x7F6FC01B,
             "EuSMQEuXbesPle6PWswanFCkeuoNEbnkgMA"),
    TESTCASE(0x52324660, 0x455CF558, 0x2F321C88, 0x5B95B7BB,
             "ua2h4CTDNpcEHtMKawWzQ5SmKFcYv6IHU96P"),
    TESTCASE(0xBDDE2BB7, 0x6EB74CB6, 0x131FA88D, 0xEA91441A,
             "bzxbD5NlFSf2204aJiy33JAeVu4zgS6ppBhoz"),
    TESTCASE(0xAAB93F93, 0x9B7935C7, 0x02230743, 0x2F4ED13F,
             "RCYdfuBLeEMDnpJ9NH33a1m9bglIuInD34bLUv"),
    TESTCASE(0x9BC5DBF1, 0xEAFEE5D4, 0x6F226D42, 0x15266383,
             "iEQiPIf5jaNiwl7bWsChqodEZXC5iCeFXHLpOZN"),
    TESTCASE(0x4B9D810A, 0x4864FF96, 0x591C7E8D, 0xAB4D9D2D,
             "ZLabuZwlTSmK4wtaZX4vJMQn9VynV8xAu30QWn47"),
    TESTCASE(0xF9AE9573, 0x86FA0C27, 0x85F009BE, 0x50C1920C,
             "NNLGzwv3EduC2e0cS8xjl4O8NlpTbSTtzzVvPqhMK"),
    TESTCASE(0x219906C0, 0x4E554E85, 0x0D508CA6, 0x59BAD101,
             "QXYtQ9Q8ctGkzCiVQqV6utNtu71eP9Cfj8PhbC8Qai"),
    TESTCASE(0x2BD0B6DF, 0xAA767F1A, 0xB5D4F541, 0xB602D797,
             "bfAFrcQqX4WXGhbig45yIJHLvXHrbTFVkVOuaDBq5q9"),
    TESTCASE(0x4C12AE5A, 0xB7EB74C6, 0xED10F197, 0x7D8E427C,
             "syQGG0Pp3dszmeEIP0CfLMoiVyUZ0ke0vAhVwKCHbrJH"),
    TESTCASE(0x8176D8B8, 0xC8ADB5F1, 0x3B11D959, 0x4994FE13,
             "SiTqcJnZFr2reFXXxuypBmXPI0CSMKLchcFkSvM166E4l"),
    TESTCASE(0x1874AFE9, 0xDF6B8E6F, 0x74064507, 0x6BDB1732,
             "NY8YtaQvFjsCsezTkMVZOdZP7hSk3rRNFWw29yAKSD1WWk"),
    TESTCASE(0xCD4CA77F, 0xA19FA88F, 0x141AFF83, 0x4C5B2FA3,
             "KxPsX271bXpbgkPh4pPIQ3VvBjwNYaTUDlameHHsjmrKIDb"),
    TESTCASE(0x5D77A74C, 0x00CEC5C0, 0x51512274, 0xBC24F5A4,
             "DP60v2RZUJAvlftKMC8xq9keGkJAzBJaqyLJWQObpo2QGG2J"),
    TESTCASE(0x8693721C, 0x40EAE64E, 0x8F2CBE7D, 0xD8BA83E1,
             "IFvjiRKQXaUaAqh5RGDtCU14hr2Lu6m2H87WhxGEqvlSFNIK0"),
    TESTCASE(0xB508BBD0, 0x0513E12A, 0xA2EC7E89, 0x00F9F917,
             "OOHZTgWzjsVJeqEKIfGnnaCOqgaung6seo1rdTAPm0TQ0Q2HX0"),
    TESTCASE(0x8CC37C94, 0xBBC835D9, 0x78C8C055, 0x79F47254,
             "zHUu6VkflLwQF1ov4cMwaDOdcsQDOiMaKfRPBqmCCMkSXuhwkQV"),
    TESTCASE(0x16B7B4C0, 0xD9FC2715, 0xC25EDDF6, 0x589C8A7C,
             "eagIxzzi1piiGCIt04uYaov5t1UtXMf3tvxEgrGF60h08g05S1ag"),
    TESTCASE(0xDCB6C216, 0x93C3FCB9, 0xE271A2B2, 0xD36EBF69,
             "UldA1RYiT2CZcxSVjFNUfbTFdi9gtdPux8QxNnzuGttz1thb9DJ4t"),
    TESTCASE(0xBA8107E5, 0x2FE4503A, 0x816F0C5B, 0x9C3CD3BB,
             "dyiSPmf1uKe9MvOwRfpoNKHuInBcjmYcNx9zFrqaEikQa8rXx5uY6T"),
    TESTCASE(0x7E807E83, 0x55C9682C, 0xEDDDC634, 0x9E22A816,
             "tM3H5bdoBxjq6bN7G1Yv8qlVCZY5gurstfjcUEu9Z6aiD3Mz7aBa9Pm"),
    TESTCASE(0x64008BFF, 0x9D251584, 0x2481666B, 0xEB105BED,
             "JYkP3Tng92YbCLTX8yCxKVuHqUV4IAh5jfPqKlUXLJVuc6F8FqxUH2pj"),
    TESTCASE(0x8F31C9BD, 0x89285E1F, 0x0C0DC570, 0x0CFE8DB3,
             "lrcQ3a7c1J8nHfirGEQaStpVAWUwSVvCJUONn7pA7mCaIkrh6qw4TQXyA"),
    TESTCASE(0xAFD350A9, 0x7E2BEE06, 0x1515D360, 0xDC3D7947,
             "g3CPerz1XRuyqgARRo18G5MJi6oYotzmSQJdNkixO0534pyMAMGvcbnlJw"),
    TESTCASE(0xC125A2EF, 0x61BF2AF1, 0x18D5A548, 0x937D271B,
             "sxXQUcEGd4Ut4NDfRwu4RTN9Ct6NDXfeNi8kHCzQrmV5Vhllbo1jjUC3KVP"),
    TESTCASE(0x68714CE0, 0xD857EAE8, 0xD1C52E96, 0x6C4A9E81,
             "RLD5vWm8bZZef0sD9rLshDXzjUpiylwjaKzObe2tIOLgwxcjdA348x3Lq3gB"),
    TESTCASE(0x6A0077D5, 0x3A220075, 0xE3E0F501, 0x81B3A16E,
             "9mnykrPN9l161zuEBk02Db4Fu008g2lmDguhsQdB7GJgV0yuszXuL8TcUdheJ"),
    TESTCASE(0xA8E0D206, 0xBFAB7ECD, 0x14C07C77, 0x3BFC4091,
             "EePNTxKilALgnV1jaoxbQtLTQ8yqYq4q7jLi1auDDkVIBSrtTzyYN7HJAh81sO"),
    TESTCASE(0x78901555, 0x36C780E7, 0xF00FC35E, 0xCADB81BF,
             "xPFRANqGeIWcKdbFk98b06W3QDEUHeFqjdFrwc6KqEfGVuEim6UIbES87Mb1Eus"),
    TESTCASE(
        0x43E65A3B, 0xC69401A8, 0x15EFB9E4, 0x931E5B36,
        "zUdm1DGV8cp7E1sr2HUzOFKao16iTxb3DPho8oTHBCug7QDKWN3uUlIRBDzzDRKD"),
    TESTCASE(
        0x52510C2B, 0x06EA5D9F, 0xB2072F34, 0x56835D4E,
        "kEUNEh88JySVSgm48ID1XnRVLu6d4K4b3fuF7NhjrsTDruFN5Zx04sOr73LHbhvLg"),
    TESTCASE(
        0x21DF417E, 0x8BA5DD3F, 0x8F2DCCF6, 0x4CBB308C,
        "OHjPaKHrT4Jmsmwrg5Z7fjvzcFhmXyOyIE1ELUfhXFgiznD73NZRDBC5f4cii4r0mO"),
    TESTCASE(
        0xFCF7467C, 0x3AFB8C56, 0xF5638529, 0x454556EC,
        "GlZvk1oCc1x5olxQcqNyMxfWuxbL7aKTCBCjtuUPSrtagKQzh7C7varmQZLKandkS6r"),
    TESTCASE(
        0xEA80A772, 0x1F4AEA7B, 0x6C47F538, 0x8E96C5DA,
        "4KoQZfwddrkj3LuPsOj6Nc6WsRABNZdx0kTb6uI8Ef4b3irKzA3IBRuiGSdsm3dRPj97"),
    TESTCASE(0x3266F48C, 0x27A1853D, 0x85249A20, 0x0F471863,
             "unPVi834yoLbowrudh0XZlOidLAP6ZXR0h5Ck3d3K8qsdubziUJo9zZLO4bykyW6T"
             "BzNI"),
    TESTCASE(0x2E7CBD90, 0x340E7CA4, 0x3F6750A2, 0x2EA95E67,
             "aL0dcyd0I5GWHItgOihrBSBMQWwgT8KT65YqeweIUnAeRvYeGDLlsOeplAYIraEKU"
             "NFm6k"),
    TESTCASE(0x41DF3045, 0x6D592260, 0x91717E45, 0x1921EBAB,
             "zB65YAy5ERxmYaqzm9u9mDJbrtZ9n9IVgRdZsHUT2c9hXPA6KSiOZEq527Ib3ABK7"
             "ncccel"),
    TESTCASE(0xFB31A55E, 0xC3A92DD7, 0x9311318C, 0x05589E7F,
             "XNG1jZVVraAlyhh4IoTuOyF6fHGlYvmkLuAa9H8x7hukzYu0Oh7Jv9MiKzSlmXWHA"
             "5W7Ka1G"),
    TESTCASE(0xE8C4A831, 0x2CD1DD0F, 0x63C254AA, 0x699B42EA,
             "dJHks7hUjWFEg8akDppoXwbhBb1JkDDP8PK75kQkq47K0mOvUCz8e0GHxWsGTSNxt"
             "22CgZYOw"),
    TESTCASE(0x7546B2CC, 0x824BCD81, 0x1D51665A, 0x382514FC,
             "pp2Y85Fo02fgDEIjsFSbZGMnc702uCcyNxcremHaNDIgVYGYZP60E1T0DrYuMrbMM"
             "9LInMnbRv"),
    TESTCASE(0x553DF40F, 0x7E41B398, 0x718FE463, 0xE2D398BC,
             "tuzYkOa7dz4IWFtFqQcKsNKD6oHqxdRtX6puCxR1uwcVevP6nr6l9xcXLbSPSnP00"
             "ZN21Yerq1J"),
    TESTCASE(0x4FF3FF92, 0x5625E15A, 0xA384D63F, 0x828C730E,
             "OTbnS8fo6i4oDt7raIigeP5MBwUYDeyh2Rz9OOWkH64EXRpvwl0Z0AInubtMWnNct"
             "d5zQPhEohBD"),
    TESTCASE(0xF065F35D, 0x818C3616, 0x5921FD19, 0x3E663898,
             "oyegc0dV1RKUU9w6PpXFnkX6J6byJSS0O0BuQ8MWdqZQ6lIcJpU7magAiE17QkvZU"
             "nvJWEoY3GC5z"),
    TESTCASE(0x9028DBAE, 0x9A3C763E, 0xE1B1A96D, 0xEA342827,
             "M5ScDjt8Gv0F8eQvRZU8prp53CaI9aLXCT5v2Q6CSd2sMbVkfExByZDozg6BmNZKV"
             "KtoqfPbdGpPu1"),
    TESTCASE(0x30D8B814, 0xF12B78C4, 0x527E1015, 0x8C5F0FE5,
             "u1ScuAVgLqxHyV7k2icxLgEGst0HkHa5l7PFi6OazdPn2DUMegC7mx1GUSdZ91k99"
             "M29u1kLGDElT66"),
    TESTCASE(0x3111C056, 0x53CBBC38, 0x1B2DD88C, 0x39CB497A,
             "0DrAwYvwNTG4j5LCrtS5aPG9OIz49GGgkeb5796m2tNTyxuDQekLDIlyGCEzI4XIc"
             "mtRPIXTirdIk7XJ"),
    TESTCASE(0xACDCE674, 0x4C8C4C3B, 0x405E8723, 0x8B2D5B5E,
             "ef9ToIGJHX556iQqXtfFELTU9Rm3TiBP3aOUhTpNUa7nqAZ8oJ95gU9NTuODDy1SK"
             "4txXuIjHxprEZJEB"),
    TESTCASE(0xCB6AB927, 0x436C6592, 0xE21BF191, 0xF3AA9DC5,
             "Qr78u7s2TWEVK8hKbf4JxasPMbTdu19tii8hejzGw3JdpjpTVJ8OHIvKxRhEMMzL0"
             "Uj9DovLCJpEujNfeV"),
    TESTCASE(0x9E62FED9, 0x072D0DB4, 0xE07F5A34, 0x5F3A5924,
             "cPor11tvb7JwrEJ0eQeU0UBMaWdB1LRZP80Qowq39XyoKgsMOHGfhheGnznYRDkUR"
             "2HDOHFywJYrBTsn7Xb"),
    TESTCASE(0x09517726, 0xCEF57592, 0xA390727F, 0xF8FA1CB1,
             "olSeNrAmXDa9xsrcCmZcEaDaQ81VeJnzSiVQSpYYk7DpP1XcHJX74G9Jz7RPunxVB"
             "6um5LdOnXtyIGX2Z0o2"),
    TESTCASE(0xAB445B8D, 0xF0D91E4C, 0x27793E99, 0x3A304D75,
             "R8IcKd8Gf4rCqOZ4LJZvl5OjO20wGX7AGMYkQNG5Rma3phEjuCujSbuOsTZDeJsmu"
             "Md6WEqqDdsLbOs3ogrxM"),
    TESTCASE(0x1D08C15C, 0x57ADF5E0, 0xE2520AF4, 0xF051427F,
             "Dm5AKYAYLpPcN056HgldMywoSeZZp8lYQnQ6JIn5oSCZuk32TtaPZc2mWgID7k34b"
             "1zKxpK42lSJk888XT7JhF"),
    TESTCASE(0x6261CB3F, 0x05F6B903, 0x6E6993D4, 0x509E73EF,
             "NffucZeh6W14sXDUKydGQE7z4RRxN1PY7Mid8sVTtrRjvqAr9grz3IE5zRq0OCmWn"
             "19JOEWi7drQOOLJoxpGnX4"),
    TESTCASE(0xCD6D68B6, 0x7E4C33CE, 0xA717F0FF, 0xAFB05E9D,
             "IWQeOuaNssgljfb5DtDAxZV1jRYPVwSYZAKXMDs2jaVF9xuP8pxF5neZUJRyI3uVW"
             "K8G9nWAaY64Du2K0mO2g8cO"),
    TESTCASE(0xB23C49C0, 0xF98FC3C0, 0xF0748BED, 0x91D3644D,
             "k1qlRHwawVcff3cgGGHu4AkWGYOkmBZxkVUZEFo8S9BJEuEonSOOTrfx34n6UCAxG"
             "5cWP3njT49ba4kyjdX4FR58g"),
    TESTCASE(0x9B8D8628, 0xC74A559B, 0x9F4BC0E9, 0xB96C25F2,
             "dqRql8HmoAuow4hoIiOEylwc8pakMAO0pghJR2d3thpsUkO9KETwORS8KQ2SBcdi9"
             "4mE6khZo30kuoiDiYcTevwU7u"),
    TESTCASE(0x163AD044, 0x73030F3D, 0xE045EE23, 0xAE4491AD,
             "MmE8rLdBPAYlKx4dtTddDR4Yp8OBYUQAJlfeLtqKKpTyNXY5o4TtYIyhdGMu9poXB"
             "xIHaxVJ7Nxbs8LFC8qvf4O8TFx"),
    TESTCASE(0x2E30E66A, 0xCF57D099, 0x9F19EBC8, 0xF2EF1999,
             "RW1ggoOeADsf5kW7u9ifGyBqKYe96QTvKMgN5oehLCR57beGr9qCDwMI8LzRxtjB3"
             "wcrOsrbP20lCjg97VEDbeTHyCsO"),
    TESTCASE(0xDEE7C62E, 0x989ADB3E, 0x16F2624E, 0x9C9B0B68,
             "8kdPTIhD0XWeafN7BuuzRK0TtooVdz3gUBTA0iGMvQq1SnookonDrzBTyXovab7QV"
             "sMpV1Bx47NhxaGrgh9KwHNSAoUsG"),
    TESTCASE(0xAC1A01AF, 0x122D1A1B, 0x1A4256EF, 0x3FAAA185,
             "pMS3Jr4qy0fui2pcQNHmkc5bS0TmF3qqEU1pyB7opVbiByDxiHYcishM3nU1LB6Ad"
             "m84epTHVDGBT2XGPQ29es7j1Rv66a"),
    TESTCASE(0xC85EA519, 0x49391103, 0x5FC68CEC, 0x489C1FA6,
             "d2IslAKdtlHJ6T8ba4pZ4WXvMhGEKkc3IhGobhIgnj1G6bad0cb4wdmMoVarJv3r7"
             "cY2dDiUPukunV5jhr6F1AKIZKmHauQ"),
    TESTCASE(0xE4345B91, 0xD94D3B24, 0xC6E6D71C, 0xB09EF1C9,
             "lwzctPypbPFBsaPK5nKX8dy1OFLGgpTH75aIQtHpul9O2A7uerAWGHPi5pgwbE5up"
             "OeqKkHyRpeN4bTUavZoQhsLdljGVa6N"),
    TESTCASE(0x1E40EF0B, 0xBF60651E, 0x0F1DB986, 0xBCD79EF1,
             "OdkVU5ZZ1xDS9zyMhFTg8CCPasc6ZE9fUNjYli11SYI0gOPwJUQA6IOuRrXfVg3ww"
             "dOXpSAexfCsx54nySrdQh18voxRKdecH"),
    TESTCASE(0xA6AE3EB1, 0x61DC65CA, 0x289D77E8, 0x0CC4C33D,
             "TAjlabpvpCE9dBU8ixmSoQtgP5odIXEcAE8DTZ5fNTVZUOexlHh1TZy35CoTEOX9i"
             "YohnfscDcoODoJta9wk6OCvoq1hbz5pGN"),
    TESTCASE(0x63A86A23, 0x42B165DB, 0xB9EFCA94, 0x4973DF7F,
             "HoEjAbfScO7kbjvxTHKpmmzaRvqN7ynO147VNeWqf43Bit3ZVtRoLEoBMPC13iX5J"
             "P5kvvgy6KAmmkeKi4zdhMMOKjGn07sQsXP"),
    TESTCASE(0xBBAA69DC, 0x02A6A3F6, 0x52787548, 0xA0291724,
             "0iVg1bQQM8HqQNXGqWPF8FF04ykspd6UNUJknxCz4nIH9X7CQiXDea7FRJ0JUO5OZ"
             "JKENLEL6xQ3ku5y8KBBTNIyExxRtfn3SWOU"),
    TESTCASE(0xF62F853C, 0x965F99A1, 0x9D3954FA, 0x08ECF4A7,
             "47zklt1T72Re9rJ1WJFDWyup6XuXwNgk5Hbq6Nh56TgTzbVErbkaMJ5ekGPwWi7rK"
             "Tx2mqlzKwmYwvIyp31IBQYOLMaOQnfhiGtjH"),
    TESTCASE(0xBC8A8370, 0x5FC2FC71, 0x3469DF9A, 0x7CDD5BBE,
             "rp5Bu5726BIy6z0Y9E4CSyG8252bdsm8PkmRHIqI4P3IYJ3I4k0zqzGIcYsvYmjLq"
             "mev5ffuqKO408iuWauZLdzPwOoLL4Ao5FPbxq"),
    TESTCASE(0xD561F576, 0x22E43A21, 0x23F036A4, 0x7B056B12,
             "HxslyleFqKgThXoMoV7A1z6xqNK3VhXWI8fI9ESCejTtWn85HwFQBx1zIxRwEdvjx"
             "G4Nx64EWbYkTH0ltItyQnqpi5szfrHE0YEUhY7"),
    TESTCASE(0x38C3848E, 0x09234B8E, 0x73446778, 0xDD335D60,
             "rk4m1SfYrCcWoPSYrSnzTLuuIEmXSX0LcoSrZdoELRVffSBXAaJtUZyYYLcTiUfQP"
             "4WudruaTD4Q6r5plZHdpCvJS3gwqBR8H7J7VGCm"),
    TESTCASE(0xE7237E08, 0x6F4F7B17, 0xED5B17B5, 0x0C7FAEB7,
             "g6AzU2WAktArsucbL6MWIU47tgsAhdxDoc3KmrscpW4byqeUYYGxXlCOvv2rEubDi"
             "DFHyHmXvQBFe1822HOMg4vVf9G9LmQui4IpriTMr"),
    TESTCASE(0x72961C58, 0x5C2BC42B, 0x8C65D6CE, 0xB0A0C872,
             "ivskCAVSoHz5RBFWun5jmVnrwxgRuS7ZxLKnhx9qF6c9Qu7Tz4rYe5NDCj45Cyuyz"
             "VJQdCccN7wJ9knVhvnITJ5N9KxbEgnB2BBsUkJOYz"),
    TESTCASE(0xE769C6F1, 0x6A2A7780, 0x867C37B7, 0xB5B56634,
             "00Qr84agVMTMi4G1VRtQrunuAzGqY09D30J36ANKA0bxewAEKH9jCeMzEElhvrXIC"
             "dwr86fhV3UwmOnhsPvWaFzmjz9Fb5lj0tImDlFO6ly"),
    TESTCASE(0x225501D1, 0x4D01487F, 0x4D4E0A9E, 0x50CC7419,
             "UnGiz3mseeYogPocAXdWGHdk12xA26jhSTFu4TA6Upao1bfqGD4nz7FnH5WQnIyed"
             "EHXTlDKNWirEPw1fARDhXSgQQtnD59CdDPCjUUomwel"),
    TESTCASE(0xEC36A8DD, 0xFC75F8E0, 0x7B6B82D2, 0xD4D18A92,
             "UKXKybnTExXhAVox22AAd03iuLepY5tLg1pBjPzmRj3SzuBjpU4l1epC2NGjHmbAP"
             "HYqKNEqoFKKVmYeNRpZ7R8A3SSc1G7p9iLgiQvhexCeD"),
    TESTCASE(0x9F1AD7F3, 0xC52B1DF3, 0x5CFB093B, 0x2B227175,
             "HYynEAw1WruJMcHcchwe6e0iz2wdWRFKROmfnkAAfCfiN0KIEMspwrS2cPTeOJief"
             "vl4aX8qDegCurOWjXoWhXRLYdp8Btnrx8M4IId7uCyewx"),
    TESTCASE(0x6114646D, 0x4D838664, 0x3D080CDB, 0xC9660A9F,
             "OwxG4rMTtqmA6QWwAn29V3MJqMYICqahNqkaSuyVUpMRFjjPeQ6NX9HVOS8EomWtD"
             "q3DmNrVnFAdLV1eu5JLGpJP3RJxieHlQ2SauadNhKGawna"),
    TESTCASE(0x5AB04CD3, 0x1C14C840, 0xC8EF162E, 0x8C23B161,
             "yrN96Qxdq3pgHmnDIhWNdykCnlx1biFWaPbyNbadeS7kQN05SLwM1PhdlGHrCzfDi"
             "KusCIdZaFJoo6vSHPMlUag8EOsAtGKAXySgaEIH2xgAuZ6N"),
    TESTCASE(0x6A113E7E, 0x5D57B1A1, 0xAFDFF1F8, 0x78EB36E7,
             "BNBn1gd9xsz8RcMeAXPstzQGcJv6DeJNYVdIXb1SyCm6i3qGeuwchr94k1KRERqIY"
             "i7ZvroGylntecFjunhwAUL6lZiXPKCDAMtB9wwnwQcUeennT"),
    TESTCASE(0xEBCE9D9B, 0x42725BB5, 0xB4835A6B, 0xCB358FD7,
             "nqmrFzi1R1dVcVz7DiJkfwjVovDH0gmD7Eq2XDURshzw3Wdf53x7BnZAGzKmdqHUC"
             "ntW5aiIUlSUkny6UamI4KY0pkx672kzzzhvpr7Ob2vDL4SriP"),
    TESTCASE(0xB5015C40, 0xC65AA411, 0xAA2178E0, 0x6C056DE1,
             "l2UdZG1zblzvXDRjyFuepYdHlvJC5sXAUQPuqQEKupoxRzpd3bjz2PqLw8TVlHEeS"
             "j7lOQGf3nSGE1KRKJcMc0AZwsmO6a8kdKyzaXIdGjWAMwk08tG"),
    TESTCASE(0x85BABA71, 0x34007647, 0x2BF1307E, 0x30796A33,
             "1hJkDIRoBuk9VMsfRzDTmYBHthT88RGb9j53kQx1N4zotBbtgq3aA5zRjH5aBwNyW"
             "j7hkLbINXvQ8a5A579ua6r00rgAItpClnUvBl5PWGjvEjk5fuPK"),
    TESTCASE(0xA8BE39D7, 0x29E4E51E, 0x091908EA, 0xDD7DBC64,
             "qnPiopeaePsrhcI2FiUuxUOlCGeHQ7NpdwTvdjDjpxAT9DgXwW7pKCS0ofuKU0utS"
             "ARBLhAAt0gubjdMGf8Lb8wbByVuP4VpFYrxtPgZ0VWNLlSm8BVqe"),
    TESTCASE(0xDA6A36DC, 0x8018B902, 0xCCB4ADF7, 0xA43F8ECB,
             "6NidrvZxiQInN99TkKUebIx6h8vegYJ6giUNWNbuaqF89FVG7PZocLuwqHM9ReGVN"
             "7nRReVSIAMcJC4oJ9BoRp9vL88MktqbTH7PEIKpww7E7FPoDgC2ms"),
    TESTCASE(0x6877FA07, 0xD846FCE3, 0x83FA6ABA, 0x794DD858,
             "uWNMpBRcVzJAc3FcUYaN6tvpAiKeHkvBIhVj2pIXQbutaDf22MLWzj10HUWMshpnv"
             "RsyLGmCw5AAXeL9M4KIqf9W5AbWrVvVFdo7LCkIodUs3BDDImMTJ5Y"),
    TESTCASE(0xD67208B4, 0xD3F23607, 0x70826F4E, 0xCB764F43,
             "LMd4BAMxLfB359jiZ0U4ZTzT3HDlMGTgrO6LF7EcfARdeFPvVtb3XMmzL1DgqIzg2"
             "j4jLzoaHqCB4u7TYRBiLAAeZAul7Dylr6DxvuCmcowBR0UmxVhRfJoq"),
    TESTCASE(0x35DD4354, 0xADBAEFA1, 0xACCEAA4F, 0x609CB5A2,
             "aXTmrkxaTvcttGu8UGpSDtZSzCwwlnDyym7bKRdUn8b4OG5Zj5hqqQ67w3UkEC7QP"
             "CG1PRTGeoS2WotGgiwYyIHWIVL8B5oQQMBXcU2pESnA5tbMA9iWhPtNR"),
    TESTCASE(0xA54CBAFA, 0x1B1574E6, 0x7849BBE5, 0xEDBC1098,
             "SXtrNyT3xELAmZJtUx9tOM7Invu1dX6jMB5FnhYInvRh2jfB5wYcQh98SZNpFdcPB"
             "L1xXh1VnvUUncJmaflPo5YiiwHAVBFPkcCuNZfHUiElAh0YPEXkJgFHlK"),
    TESTCASE(0xA5DB7048, 0x26D9F12E, 0x8C3C5658, 0x18436436,
             "hTRhjMgK40USYKweqxwCOQCp7Evtcg716ZdWaQ8Gcg5hnPskRl2AAUxCRG1CXEbOH"
             "j6oho0KEsdpEhaAWN7Ga28Wuy2pBvyqbziZsPOFLGjXW2PcmaaerttOCXn"),
    TESTCASE(0x9D4C5DD4, 0xDD04E1F3, 0x86B0AACC, 0x10F9DB23,
             "teWBrKl02Tf4c4miKdHnOgqMEVjQK03ySZbUq9RYbFzql0Bp2eKXL7hlNcxuqDoyX"
             "0N0GrIuhPVvZ2l1aeSJYlCGDTog7DKTjW770bUX3XWdG6asBkR1UTOt6T7z"),
    TESTCASE(0x03211FDA, 0xE3CEC4C8, 0x4FCCDD95, 0xE7F9EE52,
             "eaQqSibvOFff5NTaCZPG8aGSrZbJJOWgcqugpiNdZ7rXUIPZgHfhRCgRGwYmyeLjy"
             "uesyPiE2f8dqEsPPLNXfcM1mtNyr7KREuJVS5xFkcDjseIcNwlFs8dlrwd9i"),
    TESTCASE(0x93153A6E, 0xE863B86B, 0xA767A7EF, 0x8F91DCBE,
             "2mg5WYU6vOsRRePoarODDSEBJW41CDt9cN4NHoKDBFFp5YX6yVylFmqRCVqwEwQ2p"
             "HO5QlAPaguJnVJhf28RyfcJItvc6uf30zXa9q9ivkygHFRSqixcn2yiaCvqUc"),
    TESTCASE(0x1C522FC8, 0x2A9FDFD1, 0x3228ACFD, 0x1B692202,
             "6ZlMjcSt4yedTYAGXrtSwKfbxfcUJKErBgrgzwOZHUESfTeXmxhFwsOoeNHPABbX3"
             "fVO38IXWNYmrLeJRDh7iMlHI4xxQeWeXrVWkjCy2aiFN9hLPyZRGMJ9r5upxjO"),
    TESTCASE(0xC8077A52, 0x7BBFC6DD, 0xE10FD5B5, 0x5CA350C6,
             "eojYUfdQ7vky8jpCUw1yBU1vHMnpZlnVpK01f1LPXVTgsBvqcNjYwdRxsBuSCnwnG"
             "CvQK1EjkHE8BTL93tpPvlJ6M1q8gUBgacf2AXNiWWu5UHGpxpyo2tZUrZwAtJrL"),
};

INSTANTIATE_TEST_SUITE_P(MD5Test, MD5Test, ::testing::ValuesIn(testCases));

TEST_P(MD5Test, CheckTestCaseSet) {
  ASSERT_NO_THROW({
    auto hash = rawspeed::md5::md5_hash(
        message, strlen(reinterpret_cast<const char*>(message)));

    ASSERT_EQ(hash, answer);
  });
}

TEST_P(MD5Test, CheckTestCaseSetInParts) {
  ASSERT_NO_THROW({
    const auto len_total = rawspeed::implicit_cast<int>(
        strlen(reinterpret_cast<const char*>(message)));
    for (int len_0 = 0; len_0 <= len_total; ++len_0) {
      for (int len_1 = 0; len_0 + len_1 <= len_total; ++len_1) {
        auto str = std::string_view(reinterpret_cast<const char*>(message));
        rawspeed::md5::MD5 hasher;
        const auto substr0 = str.substr(/*pos=*/0, /*n=*/len_0);
        str.remove_prefix(substr0.size());
        const auto substr1 = str.substr(/*pos=*/0, /*n=*/len_1);
        str.remove_prefix(substr1.size());
        const auto substr2 = str;
        hasher.take(reinterpret_cast<const uint8_t*>(substr0.data()),
                    substr0.size());
        hasher.take(reinterpret_cast<const uint8_t*>(substr1.data()),
                    substr1.size());
        hasher.take(reinterpret_cast<const uint8_t*>(substr2.data()),
                    substr2.size());
        rawspeed::md5::MD5Hasher::state_type hash = hasher.flush();
        ASSERT_EQ(hash, answer);
      }
    }
  });
}
