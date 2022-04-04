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

#include "metadata/Camera.h" // for Hints
#include <gtest/gtest.h>     // for AssertionResult, Test, Message, TestPar...
#include <string>            // for basic_string, string, to_string
#include <tuple>             // for get, tuple

using rawspeed::Hints;
using std::string;
using std::to_string;

namespace rawspeed_test {

TEST(CameraTest, HintsEmpty) {
  Hints hints;
  ASSERT_FALSE(hints.has("something"));
}

TEST(CameraTest, HintsGetDefault) {
  Hints hints;
  ASSERT_FALSE(hints.get("something", false));
  ASSERT_TRUE(hints.get("something", true));
  ASSERT_EQ(hints.get("something", string("the default value")),
            "the default value");
  ASSERT_EQ(hints.get("something", 42), 42);
  ASSERT_EQ(hints.get("something", -84), -84);
  ASSERT_EQ(hints.get("something", 3.14f), 3.14f);
  ASSERT_EQ(hints.get("something", 2.71), 2.71);
}

TEST(CameraTest, HintsAssignmentConstructor) {
  const string key("something");

  Hints hints;
  ASSERT_FALSE(hints.has(key));

  hints.add(key, "indeed");
  ASSERT_TRUE(hints.has(key));

  const Hints hints2(hints);
  ASSERT_TRUE(hints2.has(key));

  const Hints hints3(hints2);
  ASSERT_TRUE(hints3.has(key));
}

TEST(CameraTest, HintsAssignment) {
  const string key("something");

  Hints hints;

  ASSERT_FALSE(hints.has(key));
  hints.add(key, "indeed");
  ASSERT_TRUE(hints.has(key));

  const Hints hints2 = hints;
  ASSERT_TRUE(hints2.has(key));

  const Hints hints3 = hints2;
  ASSERT_TRUE(hints3.has(key));
}

TEST(CameraTest, HintsAdd) {
  Hints hints;
  const string key("something"), value("whocares");
  ASSERT_FALSE(hints.has(key));
  hints.add(key, value);
  ASSERT_TRUE(hints.has(key));
  ASSERT_EQ(hints.get(key, string()), value);
}

TEST(CameraTest, HintsInt) {
  Hints hints;
  const int val = -42;
  const string key("thenum"), value(to_string(val));
  ASSERT_FALSE(hints.has(key));
  hints.add(key, value);
  ASSERT_TRUE(hints.has(key));
  ASSERT_EQ(hints.get(key, 0), val);
}

TEST(CameraTest, HintsUInt) {
  Hints hints;
  const unsigned int val = 84;
  const string key("thenum"), value(to_string(val));
  ASSERT_FALSE(hints.has(key));
  hints.add(key, value);
  ASSERT_TRUE(hints.has(key));
  ASSERT_EQ(hints.get(key, 0U), val);
}

TEST(CameraTest, HintsFloat) {
  Hints hints;
  const float val = 3.14f;
  const string key("theflt"), value(to_string(val));
  ASSERT_FALSE(hints.has(key));
  hints.add(key, value);
  ASSERT_TRUE(hints.has(key));
  ASSERT_EQ(hints.get(key, 0.0F), val);
}

TEST(CameraTest, HintsDouble) {
  Hints hints;
  const double val = 2.71;
  const string key("thedbl"), value(to_string(val));
  ASSERT_FALSE(hints.has(key));
  hints.add(key, value);
  ASSERT_TRUE(hints.has(key));
  ASSERT_EQ(hints.get(key, 0.0), val);
}

TEST(BoolHintTest, HintsBoolTrue) {
  Hints hints;

  const string key("key1");
  ASSERT_FALSE(hints.has(key));
  hints.add(key, "true");
  ASSERT_TRUE(hints.has(key));
  ASSERT_TRUE(hints.get(key, false));
}

class BoolHintTest : public ::testing::TestWithParam<std::tuple<string>> {
protected:
  void SetUp() override { notTrue = std::get<0>(GetParam()); }
  string notTrue;
};
INSTANTIATE_TEST_CASE_P(NotTrue, BoolHintTest,
                        ::testing::Values("True", "false", "False", "", "_"));

TEST_P(BoolHintTest, HintsBool) {
  Hints hints;

  const string key("key");
  ASSERT_FALSE(hints.has(key));
  hints.add(key, notTrue);
  ASSERT_TRUE(hints.has(key));
  ASSERT_FALSE(hints.get(key, true));
}

} // namespace rawspeed_test
