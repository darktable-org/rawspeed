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

#include "common/Cpuid.h" // for Cpuid
#include <cstdlib>        // for exit
#include <gtest/gtest.h>  // for AssertionResult, DeathTest, Test, AssertHe...
#include <string>         // for string

using rawspeed::Cpuid;

namespace rawspeed_test {

TEST(CpuidDeathTest, SSE2Test) {
  ASSERT_EXIT(
      {
#if defined(__SSE2__)
        ASSERT_TRUE(Cpuid::SSE2());
#else
        ASSERT_FALSE(Cpuid::SSE2());
#endif
        exit(0);
      },
      ::testing::ExitedWithCode(0), "");
}

} // namespace rawspeed_test
