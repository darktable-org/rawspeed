/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2016 Roman Lebedev

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

#include "CameraMetadataException.h"
#include "CiffParserException.h"
#include "FileIOException.h"
#include "IOException.h"
#include "RawDecoderException.h"
#include "TiffParserException.h"
#include <gmock/gmock.h>
#include <memory>

using namespace std;
using namespace RawSpeed;

static const std::string msg("my very Smart error Message #1 !");

template <class T> class ExceptionsTest : public testing::Test {};

typedef testing::Types<CameraMetadataException, CiffParserException,
                       FileIOException, IOException, RawDecoderException,
                       TiffParserException>
    Classes;

TYPED_TEST_CASE(ExceptionsTest, Classes);

TYPED_TEST(ExceptionsTest, Constructor) {
  ASSERT_NO_THROW({ TypeParam Exception(msg); });
  ASSERT_NO_THROW({ unique_ptr<TypeParam> Exception(new TypeParam(msg)); });
}

TYPED_TEST(ExceptionsTest, AssignmentConstructor) {
  ASSERT_NO_THROW({
    const TypeParam ExceptionOne(msg);
    TypeParam ExceptionTwo(ExceptionOne); // NOLINT trying to test the copy
  });

  ASSERT_NO_THROW({
    const unique_ptr<const TypeParam> ExceptionOne(new TypeParam(msg));
    unique_ptr<TypeParam> ExceptionTwo(new TypeParam(*ExceptionOne));
  });

  ASSERT_NO_THROW({
    const TypeParam ExceptionOne(msg);
    unique_ptr<TypeParam> ExceptionTwo(new TypeParam(ExceptionOne));
  });

  ASSERT_NO_THROW({
    const unique_ptr<const TypeParam> ExceptionOne(new TypeParam(msg));
    TypeParam ExceptionTwo(*ExceptionOne);
  });
}

TYPED_TEST(ExceptionsTest, Throw) {
  ASSERT_ANY_THROW(throw TypeParam(msg));
  EXPECT_THROW(throw TypeParam(msg), TypeParam);
  EXPECT_THROW(throw TypeParam(msg), std::runtime_error);

  ASSERT_ANY_THROW({
    std::unique_ptr<TypeParam> Exception(new TypeParam(msg));
    throw * Exception.get();
  });
  EXPECT_THROW(
      {
        std::unique_ptr<TypeParam> Exception(new TypeParam(msg));
        throw * Exception.get();
      },
      std::runtime_error);
  EXPECT_THROW(
      {
        std::unique_ptr<TypeParam> Exception(new TypeParam(msg));
        throw * Exception.get();
      },
      TypeParam);
}

TYPED_TEST(ExceptionsTest, ThrowMessage) {
  try {
    throw TypeParam(msg);
  } catch (std::exception &ex) {
    ASSERT_THAT(ex.what(), testing::HasSubstr(msg));
    EXPECT_THAT(ex.what(), testing::StrEq(msg));
  }

  try {
    std::unique_ptr<TypeParam> Exception(new TypeParam(msg));
    throw * Exception.get();
  } catch (std::exception &ex) {
    ASSERT_THAT(ex.what(), testing::HasSubstr(msg));
    EXPECT_THAT(ex.what(), testing::StrEq(msg));
  }

  try {
    std::unique_ptr<TypeParam> ExceptionOne(new TypeParam(msg));
    const std::unique_ptr<const TypeParam> ExceptionTwo(new TypeParam(msg));
    throw * ExceptionTwo.get();
  } catch (std::exception &ex) {
    ASSERT_THAT(ex.what(), testing::HasSubstr(msg));
    EXPECT_THAT(ex.what(), testing::StrEq(msg));
  }

  try {
    const TypeParam ExceptionOne(msg);
    std::unique_ptr<TypeParam> ExceptionTwo(new TypeParam(msg));
    throw * ExceptionTwo.get();
  } catch (std::exception &ex) {
    ASSERT_THAT(ex.what(), testing::HasSubstr(msg));
    EXPECT_THAT(ex.what(), testing::StrEq(msg));
  }
}

TEST(CameraMetadataException, ThrowCMETest) {
  ASSERT_ANY_THROW(ThrowCME("%s", msg.c_str()));
  EXPECT_THROW(ThrowCME("%s", msg.c_str()), std::runtime_error);
  EXPECT_THROW(ThrowCME("%s", msg.c_str()), CameraMetadataException);
}

TEST(CameraMetadataException, ThrowCMETestMessage) {
  try {
    ThrowCME("%s", msg.c_str());
  } catch (std::exception &ex) {
    ASSERT_THAT(ex.what(), testing::HasSubstr(msg));
    EXPECT_THAT(ex.what(), testing::StrEq(msg));
  }
}

TEST(CiffParserException, ThrowCPETest) {
  ASSERT_ANY_THROW(ThrowCPE("%s", msg.c_str()));
  EXPECT_THROW(ThrowCPE("%s", msg.c_str()), std::runtime_error);
  EXPECT_THROW(ThrowCPE("%s", msg.c_str()), CiffParserException);
}

TEST(CiffParserException, ThrowCPETestMessage) {
  try {
    ThrowCPE("%s", msg.c_str());
  } catch (std::exception &ex) {
    ASSERT_THAT(ex.what(), testing::HasSubstr(msg));
    EXPECT_THAT(ex.what(), testing::StrEq(msg));
  }
}

TEST(FileIOException, ThrowFIETest) {
  ASSERT_ANY_THROW(ThrowFIE("%s", msg.c_str()));
  EXPECT_THROW(ThrowFIE("%s", msg.c_str()), std::runtime_error);
  EXPECT_THROW(ThrowFIE("%s", msg.c_str()), FileIOException);
}

TEST(FileIOException, ThrowFIETestMessage) {
  try {
    ThrowFIE("%s", msg.c_str());
  } catch (std::exception &ex) {
    ASSERT_THAT(ex.what(), testing::HasSubstr(msg));
    EXPECT_THAT(ex.what(), testing::StrEq(msg));
  }
}

TEST(IOException, ThrowIOETest) {
  ASSERT_ANY_THROW(ThrowIOE("%s", msg.c_str()));
  EXPECT_THROW(ThrowIOE("%s", msg.c_str()), std::runtime_error);
  EXPECT_THROW(ThrowIOE("%s", msg.c_str()), IOException);
}

TEST(IOException, ThrowIOETestMessage) {
  try {
    ThrowIOE("%s", msg.c_str());
  } catch (std::exception &ex) {
    ASSERT_THAT(ex.what(), testing::HasSubstr(msg));
    EXPECT_THAT(ex.what(), testing::StrEq(msg));
  }
}

TEST(RawDecoderException, ThrowRDETest) {
  ASSERT_ANY_THROW(ThrowRDE("%s", msg.c_str()));
  EXPECT_THROW(ThrowRDE("%s", msg.c_str()), std::runtime_error);
  EXPECT_THROW(ThrowRDE("%s", msg.c_str()), RawDecoderException);
}

TEST(RawDecoderException, ThrowRDETestMessage) {
  try {
    ThrowRDE("%s", msg.c_str());
  } catch (std::exception &ex) {
    ASSERT_THAT(ex.what(), testing::HasSubstr(msg));
    EXPECT_THAT(ex.what(), testing::StrEq(msg));
  }
}

TEST(TiffParserException, ThrowTPETest) {
  ASSERT_ANY_THROW(ThrowTPE("%s", msg.c_str()));
  EXPECT_THROW(ThrowTPE("%s", msg.c_str()), std::runtime_error);
  EXPECT_THROW(ThrowTPE("%s", msg.c_str()), TiffParserException);
}

TEST(TiffParserException, ThrowTPEMessage) {
  try {
    ThrowTPE("%s", msg.c_str());
  } catch (std::exception &ex) {
    ASSERT_THAT(ex.what(), testing::HasSubstr(msg));
    EXPECT_THAT(ex.what(), testing::StrEq(msg));
  }
}
