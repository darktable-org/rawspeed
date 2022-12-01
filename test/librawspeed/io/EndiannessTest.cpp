/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2017 Roman Lebedev

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

#include "EndiannessTest.h"
#include "io/Endianness.h" // for getHostEndianness, Endianness, getByteSwa...
#include <cstring>         // for memcpy, memcmp
#include <gtest/gtest.h>   // for ParamIteratorInterface, Message, TestPart...
#include <iomanip>         // for setfill, setw, _Setw, _Setfill
#include <iostream>        // for hex, endl, ostream
#include <memory>          // for allocator

using rawspeed::Endianness;
using rawspeed::getBE;
using rawspeed::getByteSwapped;
using rawspeed::getHostEndianness;
using rawspeed::getHostEndiannessRuntime;
using rawspeed::getLE;
using rawspeed::getU16BE;
using rawspeed::getU16LE;
using rawspeed::getU32BE;
using rawspeed::getU32LE;
using std::setfill;
using std::setw;

namespace rawspeed_test {

TEST(EndiannessTest, getHostEndiannessTests) {
#if defined(__BYTE_ORDER__)
  ASSERT_EQ(getHostEndiannessRuntime(), getHostEndianness());
#endif
}

/*
#!/bin/bash
d=16 # squared, how many samples
# B=2 # sizeof, bytes
b=x # print format
p="0x" # print prefix
function proc {
  echo "$1" | od -A n --endian="$2" -t $3$B -N $B -w$B | tr -d ''
}
function pp {
  v=$(proc "$1" "$2" "$b")
  echo $p$v
}
for i in $(seq $d)
do
  for j in $(seq $d);
  do
    v=$(dd if=/dev/urandom bs=$B conv=sparse count=1 status=none)
    x=$(pp "$v" little);
    y=$(pp "$v" big);
    echo "{$x, $y},";
  done;
done;
*/

#define setupHex setfill('0') << setw(2 * sizeof(T)) << std::hex

template <typename T>
::std::ostream& operator<<(::std::ostream& os, const intPair<T>& p) {
  ::testing::Message msg;
  msg << "(0x" << setupHex << p.first << ", 0x" << setupHex << p.second << ")";

  return os << msg;
}

// no polymorphic lambda till c++14
struct HexEquals {
  template <typename T>
  ::testing::AssertionResult operator()(const char* darg1, const char* darg2,
                                        const T& arg1, const T& arg2) {
    if (memcmp(&arg1, &arg2, sizeof(T)) == 0)
      return ::testing::AssertionSuccess();

    ::testing::Message msg;
    msg << "      Expected: " << darg1 << std::endl;
    msg << "      Which is: " << setupHex << arg1 << std::endl;
    msg << "To be equal to: " << darg2 << std::endl;
    msg << "      Which is: " << setupHex << arg2;

    return ::testing::AssertionFailure() << msg;
  }
};

#undef setupHex

template <class T1, class T2>
class AbstractGetByteSwappedTest : public ::testing::TestWithParam<T1> {
protected:
  AbstractGetByteSwappedTest() = default;
  virtual void SetUp() {
    auto p = this->GetParam();
    auto v = std::get<0>(p);

    // swap them around? the test is symmetrical
    if (std::get<1>(p)) {
      memcpy(&in, &(v.first), sizeof(T2));
      memcpy(&expected, &(v.second), sizeof(T2));
    } else {
      memcpy(&in, &(v.second), sizeof(T2));
      memcpy(&expected, &(v.first), sizeof(T2));
    }
  }
  T2 getByteSwappedT(const void* data, bool bswap) {
    return getByteSwapped<T2>(data, bswap);
  }
  T2 getBEt(const void* data) { return getBE<T2>(data); }
  T2 getLEt(const void* data) { return getLE<T2>(data); }

  T2 in;       // input
  T2 expected; // expected output
};

/*
B=2 # sizeof, bytes
*/
class ushort16Test
    : public AbstractGetByteSwappedTest<ushort16TType, uint16_t> {};
INSTANTIATE_TEST_CASE_P(ushort16Test, ushort16Test,
                        ::testing::Combine(::testing::ValuesIn(ushort16Values),
                                           ::testing::Bool()));
TEST_P(ushort16Test, swap) {
  ASSERT_PRED_FORMAT2(HexEquals{}, getByteSwapped(in), expected);
}
TEST_P(ushort16Test, NOP) {
  ASSERT_PRED_FORMAT2(HexEquals{}, getByteSwappedT(&in, false), in);
}
TEST_P(ushort16Test, typedSwap) {
  ASSERT_PRED_FORMAT2(HexEquals{}, getByteSwappedT(&in, true), expected);
}
TEST_P(ushort16Test, get) {
  if (getHostEndianness() == Endianness::little) {
    ASSERT_PRED_FORMAT2(HexEquals{}, getBEt(&in), expected);
  } else if (getHostEndianness() == Endianness::big) {
    ASSERT_PRED_FORMAT2(HexEquals{}, getLEt(&in), expected);
  }
}
TEST_P(ushort16Test, getNOP) {
  if (getHostEndianness() == Endianness::little) {
    ASSERT_PRED_FORMAT2(HexEquals{}, getLEt(&in), in);
  } else if (getHostEndianness() == Endianness::big) {
    ASSERT_PRED_FORMAT2(HexEquals{}, getBEt(&in), in);
  }
}

TEST_P(ushort16Test, getU16) {
  if (getHostEndianness() == Endianness::little) {
    ASSERT_PRED_FORMAT2(HexEquals{}, getU16BE(&in), expected);
  } else if (getHostEndianness() == Endianness::big) {
    ASSERT_PRED_FORMAT2(HexEquals{}, getU16LE(&in), expected);
  }
}
TEST_P(ushort16Test, getU16NOP) {
  if (getHostEndianness() == Endianness::little) {
    ASSERT_PRED_FORMAT2(HexEquals{}, getU16LE(&in), in);
  } else if (getHostEndianness() == Endianness::big) {
    ASSERT_PRED_FORMAT2(HexEquals{}, getU16BE(&in), in);
  }
}

class short16Test : public AbstractGetByteSwappedTest<ushort16TType, int16_t> {
};
INSTANTIATE_TEST_CASE_P(short16Test, short16Test,
                        ::testing::Combine(::testing::ValuesIn(ushort16Values),
                                           ::testing::Bool()));
TEST_P(short16Test, swap) {
  ASSERT_PRED_FORMAT2(HexEquals{}, getByteSwapped(in), expected);
}
TEST_P(short16Test, NOP) {
  ASSERT_PRED_FORMAT2(HexEquals{}, getByteSwappedT(&in, false), in);
}
TEST_P(short16Test, typedSwap) {
  ASSERT_PRED_FORMAT2(HexEquals{}, getByteSwappedT(&in, true), expected);
}
TEST_P(short16Test, get) {
  if (getHostEndianness() == Endianness::little) {
    ASSERT_PRED_FORMAT2(HexEquals{}, getBEt(&in), expected);
  } else if (getHostEndianness() == Endianness::big) {
    ASSERT_PRED_FORMAT2(HexEquals{}, getLEt(&in), expected);
  }
}
TEST_P(short16Test, getNOP) {
  if (getHostEndianness() == Endianness::little) {
    ASSERT_PRED_FORMAT2(HexEquals{}, getLEt(&in), in);
  } else if (getHostEndianness() == Endianness::big) {
    ASSERT_PRED_FORMAT2(HexEquals{}, getBEt(&in), in);
  }
}

/*
B=4 # sizeof, bytes
*/
class uint32Test : public AbstractGetByteSwappedTest<uint32TType, uint32_t> {};
INSTANTIATE_TEST_CASE_P(uint32Test, uint32Test,
                        ::testing::Combine(::testing::ValuesIn(uint32Values),
                                           ::testing::Bool()));
TEST_P(uint32Test, swap) {
  ASSERT_PRED_FORMAT2(HexEquals{}, getByteSwapped(in), expected);
}
TEST_P(uint32Test, NOP) {
  ASSERT_PRED_FORMAT2(HexEquals{}, getByteSwappedT(&in, false), in);
}
TEST_P(uint32Test, typedSwap) {
  ASSERT_PRED_FORMAT2(HexEquals{}, getByteSwappedT(&in, true), expected);
}
TEST_P(uint32Test, get) {
  if (getHostEndianness() == Endianness::little) {
    ASSERT_PRED_FORMAT2(HexEquals{}, getBEt(&in), expected);
  } else if (getHostEndianness() == Endianness::big) {
    ASSERT_PRED_FORMAT2(HexEquals{}, getLEt(&in), expected);
  }
}
TEST_P(uint32Test, getNOP) {
  if (getHostEndianness() == Endianness::little) {
    ASSERT_PRED_FORMAT2(HexEquals{}, getLEt(&in), in);
  } else if (getHostEndianness() == Endianness::big) {
    ASSERT_PRED_FORMAT2(HexEquals{}, getBEt(&in), in);
  }
}

TEST_P(uint32Test, getU32) {
  if (getHostEndianness() == Endianness::little) {
    ASSERT_PRED_FORMAT2(HexEquals{}, getU32BE(&in), expected);
  } else if (getHostEndianness() == Endianness::big) {
    ASSERT_PRED_FORMAT2(HexEquals{}, getU32LE(&in), expected);
  }
}
TEST_P(uint32Test, getU32NOP) {
  if (getHostEndianness() == Endianness::little) {
    ASSERT_PRED_FORMAT2(HexEquals{}, getU32LE(&in), in);
  } else if (getHostEndianness() == Endianness::big) {
    ASSERT_PRED_FORMAT2(HexEquals{}, getU32BE(&in), in);
  }
}

class int32Test : public AbstractGetByteSwappedTest<uint32TType, int32_t> {};
INSTANTIATE_TEST_CASE_P(int32Test, int32Test,
                        ::testing::Combine(::testing::ValuesIn(uint32Values),
                                           ::testing::Bool()));
TEST_P(int32Test, swap) {
  ASSERT_PRED_FORMAT2(HexEquals{}, getByteSwapped(in), expected);
}
TEST_P(int32Test, NOP) {
  ASSERT_PRED_FORMAT2(HexEquals{}, getByteSwappedT(&in, false), in);
}
TEST_P(int32Test, typedSwap) {
  ASSERT_PRED_FORMAT2(HexEquals{}, getByteSwappedT(&in, true), expected);
}
TEST_P(int32Test, get) {
  if (getHostEndianness() == Endianness::little) {
    ASSERT_PRED_FORMAT2(HexEquals{}, getBEt(&in), expected);
  } else if (getHostEndianness() == Endianness::big) {
    ASSERT_PRED_FORMAT2(HexEquals{}, getLEt(&in), expected);
  }
}
TEST_P(int32Test, getNOP) {
  if (getHostEndianness() == Endianness::little) {
    ASSERT_PRED_FORMAT2(HexEquals{}, getLEt(&in), in);
  } else if (getHostEndianness() == Endianness::big) {
    ASSERT_PRED_FORMAT2(HexEquals{}, getBEt(&in), in);
  }
}

/*
B=8 # sizeof, bytes
*/
class uint64Test : public AbstractGetByteSwappedTest<uint64TType, uint64_t> {};
INSTANTIATE_TEST_CASE_P(uint64Test, uint64Test,
                        ::testing::Combine(::testing::ValuesIn(uint64Values),
                                           ::testing::Bool()));
TEST_P(uint64Test, swap) {
  ASSERT_PRED_FORMAT2(HexEquals{}, getByteSwapped(in), expected);
}
TEST_P(uint64Test, NOP) {
  ASSERT_PRED_FORMAT2(HexEquals{}, getByteSwappedT(&in, false), in);
}
TEST_P(uint64Test, typedSwap) {
  ASSERT_PRED_FORMAT2(HexEquals{}, getByteSwappedT(&in, true), expected);
}
TEST_P(uint64Test, get) {
  if (getHostEndianness() == Endianness::little) {
    ASSERT_PRED_FORMAT2(HexEquals{}, getBEt(&in), expected);
  } else if (getHostEndianness() == Endianness::big) {
    ASSERT_PRED_FORMAT2(HexEquals{}, getLEt(&in), expected);
  }
}
TEST_P(uint64Test, getNOP) {
  if (getHostEndianness() == Endianness::little) {
    ASSERT_PRED_FORMAT2(HexEquals{}, getLEt(&in), in);
  } else if (getHostEndianness() == Endianness::big) {
    ASSERT_PRED_FORMAT2(HexEquals{}, getBEt(&in), in);
  }
}

class floatTest : public AbstractGetByteSwappedTest<uint32TType, float> {};
INSTANTIATE_TEST_CASE_P(floatTest, floatTest,
                        ::testing::Combine(::testing::ValuesIn(uint32Values),
                                           ::testing::Bool()));
TEST_P(floatTest, swap) {
  ASSERT_PRED_FORMAT2(HexEquals{}, getByteSwapped(in), expected);
}
TEST_P(floatTest, NOP) {
  ASSERT_PRED_FORMAT2(HexEquals{}, getByteSwappedT(&in, false), in);
}
TEST_P(floatTest, typedSwap) {
  ASSERT_PRED_FORMAT2(HexEquals{}, getByteSwappedT(&in, true), expected);
}
TEST_P(floatTest, get) {
  if (getHostEndianness() == Endianness::little) {
    ASSERT_PRED_FORMAT2(HexEquals{}, getBEt(&in), expected);
  } else if (getHostEndianness() == Endianness::big) {
    ASSERT_PRED_FORMAT2(HexEquals{}, getLEt(&in), expected);
  }
}
TEST_P(floatTest, getNOP) {
  if (getHostEndianness() == Endianness::little) {
    ASSERT_PRED_FORMAT2(HexEquals{}, getLEt(&in), in);
  } else if (getHostEndianness() == Endianness::big) {
    ASSERT_PRED_FORMAT2(HexEquals{}, getBEt(&in), in);
  }
}

class doubleTest : public AbstractGetByteSwappedTest<uint64TType, double> {};
INSTANTIATE_TEST_CASE_P(doubleTest, doubleTest,
                        ::testing::Combine(::testing::ValuesIn(uint64Values),
                                           ::testing::Bool()));
TEST_P(doubleTest, swap) {
  ASSERT_PRED_FORMAT2(HexEquals{}, getByteSwapped(in), expected);
}
TEST_P(doubleTest, NOP) {
  ASSERT_PRED_FORMAT2(HexEquals{}, getByteSwappedT(&in, false), in);
}
TEST_P(doubleTest, typedSwap) {
  ASSERT_PRED_FORMAT2(HexEquals{}, getByteSwappedT(&in, true), expected);
}
TEST_P(doubleTest, get) {
  if (getHostEndianness() == Endianness::little) {
    ASSERT_PRED_FORMAT2(HexEquals{}, getBEt(&in), expected);
  } else if (getHostEndianness() == Endianness::big) {
    ASSERT_PRED_FORMAT2(HexEquals{}, getLEt(&in), expected);
  }
}
TEST_P(doubleTest, getNOP) {
  if (getHostEndianness() == Endianness::little) {
    ASSERT_PRED_FORMAT2(HexEquals{}, getLEt(&in), in);
  } else if (getHostEndianness() == Endianness::big) {
    ASSERT_PRED_FORMAT2(HexEquals{}, getBEt(&in), in);
  }
}

} // namespace rawspeed_test
