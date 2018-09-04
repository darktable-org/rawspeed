/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2018 Roman Lebedev

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

#include "common/ChecksumFile.h"      // for ChecksumFile
#include "common/RawspeedException.h" // for RawspeedException
#include <cstdlib>                    // for exit
#include <gtest/gtest.h> // for AssertionResult, DeathTest, Test, AssertHe...
#include <string>        // for string

using rawspeed::ParseChecksumFileContent;

namespace rawspeed_test {

TEST(ParseChecksumFileContentTest, Empty) {
  const auto Content = ParseChecksumFileContent({}, {});
  ASSERT_TRUE(Content.empty());
}

TEST(ParseChecksumFileContentTest, ShortLine) {
  auto gen = [](int len) {
    return ParseChecksumFileContent(std::string(len, ' '), {});
  };
  EXPECT_THROW(gen(41), rawspeed::RawspeedException);
  EXPECT_THROW(gen(42), rawspeed::RawspeedException);
  EXPECT_NO_THROW(gen(43));
}

TEST(ParseChecksumFileContentTest, Lines) {
  const auto OneLine = std::string(43, ' ');

  auto Content = ParseChecksumFileContent(OneLine, {});
  ASSERT_FALSE(Content.empty());
  ASSERT_EQ(Content.size(), 1);

  Content = ParseChecksumFileContent(OneLine + std::string("\n") + OneLine, {});
  ASSERT_FALSE(Content.empty());
  ASSERT_EQ(Content.size(), 2);

  Content = ParseChecksumFileContent(
      OneLine + std::string("\n") + OneLine + std::string("\n"), {});
  ASSERT_FALSE(Content.empty());
  ASSERT_EQ(Content.size(), 2);
}

TEST(ParseChecksumFileContentTest, TheTest) {
  const std::string testLine = "0000000000000000000000000000000000000000  file";

  auto Content = ParseChecksumFileContent(testLine, "");
  ASSERT_FALSE(Content.empty());
  ASSERT_EQ(Content.size(), 1);
  ASSERT_EQ(Content.front().RelFileName, "file");
  ASSERT_EQ(Content.front().FullFileName, "/file");

  Content = ParseChecksumFileContent(testLine, "dir");
  ASSERT_FALSE(Content.empty());
  ASSERT_EQ(Content.size(), 1);
  ASSERT_EQ(Content.front().RelFileName, "file");
  ASSERT_EQ(Content.front().FullFileName, "dir/file");
}

} // namespace rawspeed_test
