/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2017 Roman Lebedev

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

#include "common/Threading.h" // for sliceUp
#include <algorithm>          // for min
#include <array>              // for array
#include <cassert>            // for assert
#include <gtest/gtest.h>      // for make_tuple, tuple, ParamIteratorInterface
#include <map>                // for _Rb_tree_const_iterator, map, map<>::c...
#include <numeric>            // for accumulate
#include <utility>            // for pair
#include <vector>             // for vector

using rawspeed::sliceUp;

namespace rawspeed_test {

// reference implementation, which is readable.
inline std::vector<unsigned> sliceUp_dumb(unsigned bucketsNum,
                                          unsigned pieces) {
  std::vector<unsigned> buckets;

  if (!bucketsNum || !pieces)
    return buckets;

  buckets.resize(std::min(bucketsNum, pieces), 0U);

  // split all the pieces between all the threads 'evenly'
  unsigned piecesLeft = pieces;
  while (piecesLeft > 0U) {
    for (auto& bucket : buckets) {
      --piecesLeft;
      ++bucket;
      if (0U == piecesLeft)
        break;
    }
  }
  assert(piecesLeft == 0U);
  assert(std::accumulate(buckets.begin(), buckets.end(), 0UL) == pieces);

  return buckets;
}

using twoValsType = std::tr1::tuple<unsigned, unsigned>;

static const std::map<twoValsType, std::array<unsigned, 4>> Expected{
    {std::make_tuple(0U, 0U), {{}}},
    {std::make_tuple(0U, 1U), {{}}},
    {std::make_tuple(0U, 2U), {{}}},
    {std::make_tuple(0U, 3U), {{}}},
    {std::make_tuple(0U, 4U), {{}}},
    {std::make_tuple(0U, 5U), {{}}},
    {std::make_tuple(0U, 6U), {{}}},
    {std::make_tuple(1U, 0U), {{}}},
    {std::make_tuple(1U, 1U), {{1U}}},
    {std::make_tuple(1U, 2U), {{2U}}},
    {std::make_tuple(1U, 3U), {{3U}}},
    {std::make_tuple(1U, 4U), {{4U}}},
    {std::make_tuple(1U, 5U), {{5U}}},
    {std::make_tuple(1U, 6U), {{6U}}},
    {std::make_tuple(2U, 0U), {{}}},
    {std::make_tuple(2U, 1U), {{1U}}},
    {std::make_tuple(2U, 2U), {{1U, 1U}}},
    {std::make_tuple(2U, 3U), {{2U, 1U}}},
    {std::make_tuple(2U, 4U), {{2U, 2U}}},
    {std::make_tuple(2U, 5U), {{3U, 2U}}},
    {std::make_tuple(2U, 6U), {{3U, 3U}}},
    {std::make_tuple(3U, 0U), {{}}},
    {std::make_tuple(3U, 1U), {{1U}}},
    {std::make_tuple(3U, 2U), {{1U, 1U}}},
    {std::make_tuple(3U, 3U), {{1U, 1U, 1U}}},
    {std::make_tuple(3U, 4U), {{2U, 1U, 1U}}},
    {std::make_tuple(3U, 5U), {{2U, 2U, 1U}}},
    {std::make_tuple(3U, 6U), {{2U, 2U, 2U}}},
    {std::make_tuple(4U, 0U), {{}}},
    {std::make_tuple(4U, 1U), {{1U}}},
    {std::make_tuple(4U, 2U), {{1U, 1U}}},
    {std::make_tuple(4U, 3U), {{1U, 1U, 1U}}},
    {std::make_tuple(4U, 4U), {{1U, 1U, 1U, 1U}}},
    {std::make_tuple(4U, 5U), {{2U, 1U, 1U, 1U}}},
    {std::make_tuple(4U, 6U), {{2U, 2U, 1U, 1U}}},

};

class SliceUpTest : public ::testing::TestWithParam<twoValsType> {
protected:
  SliceUpTest() = default;
  virtual void SetUp() {
    threads = std::tr1::get<0>(GetParam());
    pieces = std::tr1::get<1>(GetParam());

    expected = Expected.find(GetParam());
    ASSERT_NE(expected, Expected.end());

    if (threads != 0) {
      ASSERT_EQ(
          std::accumulate(expected->second.begin(), expected->second.end(), 0U),
          pieces);
    }
  }

  void Check(std::vector<unsigned> got) {
    for (unsigned i = 0; i < 1; i++) {
      if (got.size() <= i)
        ASSERT_EQ(expected->second[i], 0);
      else
        ASSERT_EQ(got[i], expected->second[i]);
    }
  }

  unsigned threads;
  unsigned pieces;
  decltype(Expected)::const_iterator expected;
};
INSTANTIATE_TEST_CASE_P(SaneValues, SliceUpTest,
                        testing::Combine(testing::Range(0U, 5U),
                                         testing::Range(0U, 7U)));

TEST_P(SliceUpTest, ReferenceTest) { Check(sliceUp_dumb(threads, pieces)); }
TEST_P(SliceUpTest, Test) { Check(sliceUp(threads, pieces)); }

class SliceUpTortureTest : public ::testing::TestWithParam<twoValsType> {
protected:
  SliceUpTortureTest() = default;
  virtual void SetUp() {
    threads = std::tr1::get<0>(GetParam());
    pieces = std::tr1::get<1>(GetParam());
  }

  unsigned threads;
  unsigned pieces;
};
INSTANTIATE_TEST_CASE_P(ManyValues, SliceUpTortureTest,
                        testing::Combine(testing::Range(0U, 17U),
                                         testing::Range(0U, 63U)));
TEST_P(SliceUpTortureTest, BruteForceTest) {
  ASSERT_EQ(sliceUp(threads, pieces), sliceUp_dumb(threads, pieces));
}

} // namespace rawspeed_test
