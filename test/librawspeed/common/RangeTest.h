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

#include "common/Range.h" // for Range
#include <gtest/gtest.h>  // for TEST, ASSERT_EQ, ASSERT_TRUE, ASSERT_FALSE
#include <set>            // for set

using rawspeed::Range;

namespace rawspeed {

template <typename T>
::std::ostream& operator<<(::std::ostream& os, const Range<T>& r) {
  if (r.begin() == r.end())
    return os << "(" << r.begin() << ".." << r.begin() << ")";
  return os << "[" << r.begin() << ".." << r.end() - 1 << "]";
}

} // namespace rawspeed

namespace rawspeed_test {

template <typename T>
::testing::AssertionResult RangeContains(const char* range_expr,
                                         const char* pos_expr,
                                         const Range<T>& r, const T& pos) {
  if (RangeContains(r, r.end()))
    return ::testing::AssertionFailure() << "Range does contain it's end()!";

  if (RangeContains(r, pos))
    return ::testing::AssertionSuccess();

  return ::testing::AssertionFailure()
         << "Range " << range_expr << " " << r << " does not contain "
         << pos_expr << " (" << pos << ")";
}

template <typename T>
::testing::AssertionResult RangeDoesntContain(const char* range_expr,
                                              const char* pos_expr,
                                              const Range<T>& r, const T& pos) {
  if (RangeContains(r, r.end()))
    return ::testing::AssertionFailure() << "Range does contain it's end()!";

  if (!RangeContains(r, pos))
    return ::testing::AssertionSuccess();

  return ::testing::AssertionFailure()
         << "Range " << range_expr << " " << r << " contains " << pos_expr
         << " (" << pos << ")";
}

template <typename T>
::testing::AssertionResult RangesOverlap(const char* m_expr, const char* n_expr,
                                         const T& lhs, const T& rhs) {
  if (!RangesOverlap(lhs, lhs) || !RangesOverlap(rhs, rhs))
    return ::testing::AssertionFailure() << "Ranges don't self-overlap!";

  if (RangesOverlap(lhs, rhs) && RangesOverlap(rhs, lhs))
    return ::testing::AssertionSuccess();

  return ::testing::AssertionFailure()
         << "Ranges " << m_expr << " and " << n_expr << " (" << lhs << " and "
         << rhs << ") do not overlap.";
}

template <typename T>
::testing::AssertionResult RangesDontOverlap(const char* m_expr,
                                             const char* n_expr, const T& lhs,
                                             const T& rhs) {
  if (!RangesOverlap(lhs, lhs) || !RangesOverlap(rhs, rhs))
    return ::testing::AssertionFailure() << "Ranges don't self-overlap!";

  if (!RangesOverlap(lhs, rhs) && !RangesOverlap(rhs, lhs))
    return ::testing::AssertionSuccess();

  return ::testing::AssertionFailure()
         << "Ranges " << m_expr << " and " << n_expr << " (" << lhs << " and "
         << rhs << ") do overlap.";
}

using twoRangesType = std::tr1::tuple<int, unsigned, int, unsigned>;
class TwoRangesTest : public ::testing::TestWithParam<twoRangesType> {
protected:
  TwoRangesTest() = default;
  virtual void SetUp() {
    r0 = Range<int>(std::tr1::get<0>(GetParam()), std::tr1::get<1>(GetParam()));
    r1 = Range<int>(std::tr1::get<2>(GetParam()), std::tr1::get<3>(GetParam()));
  }

  Range<int> r0;
  Range<int> r1;
};
INSTANTIATE_TEST_CASE_P(Unsigned, TwoRangesTest,
                        testing::Combine(testing::Range(0, 3),
                                         testing::Range(0U, 3U),
                                         testing::Range(0, 3),
                                         testing::Range(0U, 3U)));

static const std::set<twoRangesType> AllOverlapped{
    {0, 0, 0, 0}, {0, 0, 0, 1}, {0, 0, 0, 2}, {0, 1, 0, 0}, {0, 1, 0, 1},
    {0, 1, 0, 2}, {0, 2, 0, 0}, {0, 2, 0, 1}, {0, 2, 0, 2}, {0, 2, 1, 0},
    {0, 2, 1, 1}, {0, 2, 1, 2}, {1, 0, 0, 2}, {1, 0, 1, 0}, {1, 0, 1, 1},
    {1, 0, 1, 2}, {1, 1, 0, 2}, {1, 1, 1, 0}, {1, 1, 1, 1}, {1, 1, 1, 2},
    {1, 2, 0, 2}, {1, 2, 1, 0}, {1, 2, 1, 1}, {1, 2, 1, 2}, {1, 2, 2, 0},
    {1, 2, 2, 1}, {1, 2, 2, 2}, {2, 0, 1, 2}, {2, 0, 2, 0}, {2, 0, 2, 1},
    {2, 0, 2, 2}, {2, 1, 1, 2}, {2, 1, 2, 0}, {2, 1, 2, 1}, {2, 1, 2, 2},
    {2, 2, 1, 2}, {2, 2, 2, 0}, {2, 2, 2, 1}, {2, 2, 2, 2},
};

} // namespace rawspeed_test
