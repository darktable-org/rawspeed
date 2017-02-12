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
#include "common/Common.h" // for int32, short16, uint32, uint64, ushort16
#include "io/Endianness.h" // for getHostEndianness, getByteSwapped, Endian...
#include <gtest/gtest.h>   // for IsNullLiteralHelper, ParamGenerator, ASSE...
#include <utility>         // for get

using namespace std;
using namespace RawSpeed;

/*
#!/bin/bash
d=16 # squared, how many samples
# B=2 # sizeof, bytes
# b=x # print format
# p="0x" # print prefix
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
    echo "make_tuple($x, $y),";
  done;
done;
*/

template <class T1, class T2>
class AbstractGetByteSwappedTest : public ::testing::TestWithParam<T1> {
protected:
  AbstractGetByteSwappedTest() = default;
  virtual void SetUp() {
    auto p = this->GetParam();
    auto v = std::tr1::get<0>(p);

    // swap them around? the test is symmetrical
    if (std::tr1::get<1>(p)) {
      in = std::tr1::get<0>(v);
      expected = std::tr1::get<1>(v);
    } else {
      in = std::tr1::get<1>(v);
      expected = std::tr1::get<0>(v);
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
b=x # print format
p="0x" # print prefix
*/
class ushort16Test
    : public AbstractGetByteSwappedTest<ushort16TType, ushort16> {};
INSTANTIATE_TEST_CASE_P(ushort16Test, ushort16Test,
                        ::testing::Combine(::testing::ValuesIn(ushort16Values),
                                           ::testing::Bool()));
TEST_P(ushort16Test, swap) { ASSERT_EQ(getByteSwapped(in), expected); }
TEST_P(ushort16Test, NOP) { ASSERT_EQ(getByteSwappedT(&in, false), in); }
TEST_P(ushort16Test, typedSwap) {
  ASSERT_EQ(getByteSwappedT(&in, true), expected);
}
TEST_P(ushort16Test, get) {
  if (getHostEndianness() == little) {
    ASSERT_EQ(getBEt(&in), expected);
  } else if (getHostEndianness() == big) {
    ASSERT_EQ(getLEt(&in), expected);
  }
}
TEST_P(ushort16Test, getNOP) {
  if (getHostEndianness() == little) {
    ASSERT_EQ(getLEt(&in), in);
  } else if (getHostEndianness() == big) {
    ASSERT_EQ(getBEt(&in), in);
  }
}

TEST_P(ushort16Test, getU16) {
  if (getHostEndianness() == little) {
    ASSERT_EQ(getU16BE(&in), expected);
  } else if (getHostEndianness() == big) {
    ASSERT_EQ(getU16LE(&in), expected);
  }
}
TEST_P(ushort16Test, getU16NOP) {
  if (getHostEndianness() == little) {
    ASSERT_EQ(getU16LE(&in), in);
  } else if (getHostEndianness() == big) {
    ASSERT_EQ(getU16BE(&in), in);
  }
}

class short16Test : public AbstractGetByteSwappedTest<ushort16TType, short16> {
};
INSTANTIATE_TEST_CASE_P(short16Test, short16Test,
                        ::testing::Combine(::testing::ValuesIn(ushort16Values),
                                           ::testing::Bool()));
TEST_P(short16Test, swap) { ASSERT_EQ(getByteSwapped(in), expected); }
TEST_P(short16Test, NOP) { ASSERT_EQ(getByteSwappedT(&in, false), in); }
TEST_P(short16Test, typedSwap) {
  ASSERT_EQ(getByteSwappedT(&in, true), expected);
}
TEST_P(short16Test, get) {
  if (getHostEndianness() == little) {
    ASSERT_EQ(getBEt(&in), expected);
  } else if (getHostEndianness() == big) {
    ASSERT_EQ(getLEt(&in), expected);
  }
}
TEST_P(short16Test, getNOP) {
  if (getHostEndianness() == little) {
    ASSERT_EQ(getLEt(&in), in);
  } else if (getHostEndianness() == big) {
    ASSERT_EQ(getBEt(&in), in);
  }
}

/*
B=4 # sizeof, bytes
b=x # print format
p="0x" # print prefix
*/
class uint32Test : public AbstractGetByteSwappedTest<uint32TType, uint32> {};
INSTANTIATE_TEST_CASE_P(uint32Test, uint32Test,
                        ::testing::Combine(::testing::ValuesIn(uint32Values),
                                           ::testing::Bool()));
TEST_P(uint32Test, swap) { ASSERT_EQ(getByteSwapped(in), expected); }
TEST_P(uint32Test, NOP) { ASSERT_EQ(getByteSwappedT(&in, false), in); }
TEST_P(uint32Test, typedSwap) {
  ASSERT_EQ(getByteSwappedT(&in, true), expected);
}
TEST_P(uint32Test, get) {
  if (getHostEndianness() == little) {
    ASSERT_EQ(getBEt(&in), expected);
  } else if (getHostEndianness() == big) {
    ASSERT_EQ(getLEt(&in), expected);
  }
}
TEST_P(uint32Test, getNOP) {
  if (getHostEndianness() == little) {
    ASSERT_EQ(getLEt(&in), in);
  } else if (getHostEndianness() == big) {
    ASSERT_EQ(getBEt(&in), in);
  }
}

TEST_P(uint32Test, getU32) {
  if (getHostEndianness() == little) {
    ASSERT_EQ(getU32BE(&in), expected);
  } else if (getHostEndianness() == big) {
    ASSERT_EQ(getU32LE(&in), expected);
  }
}
TEST_P(uint32Test, getU32NOP) {
  if (getHostEndianness() == little) {
    ASSERT_EQ(getU32LE(&in), in);
  } else if (getHostEndianness() == big) {
    ASSERT_EQ(getU32BE(&in), in);
  }
}

class int32Test : public AbstractGetByteSwappedTest<uint32TType, int32> {};
INSTANTIATE_TEST_CASE_P(int32Test, int32Test,
                        ::testing::Combine(::testing::ValuesIn(uint32Values),
                                           ::testing::Bool()));
TEST_P(int32Test, swap) { ASSERT_EQ(getByteSwapped(in), expected); }
TEST_P(int32Test, NOP) { ASSERT_EQ(getByteSwappedT(&in, false), in); }
TEST_P(int32Test, typedSwap) {
  ASSERT_EQ(getByteSwappedT(&in, true), expected);
}
TEST_P(int32Test, get) {
  if (getHostEndianness() == little) {
    ASSERT_EQ(getBEt(&in), expected);
  } else if (getHostEndianness() == big) {
    ASSERT_EQ(getLEt(&in), expected);
  }
}
TEST_P(int32Test, getNOP) {
  if (getHostEndianness() == little) {
    ASSERT_EQ(getLEt(&in), in);
  } else if (getHostEndianness() == big) {
    ASSERT_EQ(getBEt(&in), in);
  }
}

/*
B=8 # sizeof, bytes
b=x # print format
p="0x" # print prefix
*/
class uint64Test : public AbstractGetByteSwappedTest<uint64TType, uint64> {};
INSTANTIATE_TEST_CASE_P(uint64Test, uint64Test,
                        ::testing::Combine(::testing::ValuesIn(uint64Values),
                                           ::testing::Bool()));
TEST_P(uint64Test, swap) { ASSERT_EQ(getByteSwapped(in), expected); }
TEST_P(uint64Test, NOP) { ASSERT_EQ(getByteSwappedT(&in, false), in); }
TEST_P(uint64Test, typedSwap) {
  ASSERT_EQ(getByteSwappedT(&in, true), expected);
}
TEST_P(uint64Test, get) {
  if (getHostEndianness() == little) {
    ASSERT_EQ(getBEt(&in), expected);
  } else if (getHostEndianness() == big) {
    ASSERT_EQ(getLEt(&in), expected);
  }
}
TEST_P(uint64Test, getNOP) {
  if (getHostEndianness() == little) {
    ASSERT_EQ(getLEt(&in), in);
  } else if (getHostEndianness() == big) {
    ASSERT_EQ(getBEt(&in), in);
  }
}

/*
B=4 # sizeof, bytes
b=f # print format
p="" # print prefix
*/
class floatTest : public AbstractGetByteSwappedTest<floatTType, float> {};
INSTANTIATE_TEST_CASE_P(floatTest, floatTest,
                        ::testing::Combine(::testing::ValuesIn(floatValues),
                                           ::testing::Bool()));
TEST_P(floatTest, swap) { ASSERT_EQ(getByteSwapped(in), expected); }
TEST_P(floatTest, NOP) { ASSERT_EQ(getByteSwappedT(&in, false), in); }
TEST_P(floatTest, typedSwap) {
  ASSERT_EQ(getByteSwappedT(&in, true), expected);
}
TEST_P(floatTest, get) {
  if (getHostEndianness() == little) {
    ASSERT_EQ(getBEt(&in), expected);
  } else if (getHostEndianness() == big) {
    ASSERT_EQ(getLEt(&in), expected);
  }
}
TEST_P(floatTest, getNOP) {
  if (getHostEndianness() == little) {
    ASSERT_EQ(getLEt(&in), in);
  } else if (getHostEndianness() == big) {
    ASSERT_EQ(getBEt(&in), in);
  }
}

/*
B=8 # sizeof, bytes
b=f # print format
p="" # print prefix
*/
class doubleTest : public AbstractGetByteSwappedTest<doubleTType, double> {};
INSTANTIATE_TEST_CASE_P(doubleTest, doubleTest,
                        ::testing::Combine(::testing::ValuesIn(doubleValues),
                                           ::testing::Bool()));
TEST_P(doubleTest, swap) { ASSERT_EQ(getByteSwapped(in), expected); }
TEST_P(doubleTest, NOP) { ASSERT_EQ(getByteSwappedT(&in, false), in); }
TEST_P(doubleTest, typedSwap) {
  ASSERT_EQ(getByteSwappedT(&in, true), expected);
}
TEST_P(doubleTest, get) {
  if (getHostEndianness() == little) {
    ASSERT_EQ(getBEt(&in), expected);
  } else if (getHostEndianness() == big) {
    ASSERT_EQ(getLEt(&in), expected);
  }
}
TEST_P(doubleTest, getNOP) {
  if (getHostEndianness() == little) {
    ASSERT_EQ(getLEt(&in), in);
  } else if (getHostEndianness() == big) {
    ASSERT_EQ(getBEt(&in), in);
  }
}
