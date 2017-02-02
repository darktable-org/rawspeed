/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2016-2017 Roman Lebedev

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

#include "decoders/RawDecoderException.h"     // for RawDecoderException (p...
#include "io/FileIOException.h"               // for FileIOException (ptr o...
#include "io/IOException.h"                   // for IOException (ptr only)
#include "metadata/CameraMetadataException.h" // for CameraMetadataExceptio...
#include "parsers/CiffParserException.h"      // for CiffParserException (p...
#include "parsers/FiffParserException.h"      // for ThrowFPE, FiffParserEx...
#include "parsers/TiffParserException.h"      // for ThrowTPE, TiffParserEx...
#include "gtest/gtest.h"                      // for gtest_ar
#include <exception>                          // for exception
#include <gmock/gmock.h> // for MakePredicateFormatterFromMatcher
#include <gtest/gtest.h> // for Message, TestPartResult, Test
#include <memory>        // for unique_ptr
#include <stdexcept>     // for runtime_error
#include <string>        // for string

using namespace std;
using namespace RawSpeed;

static const std::string msg("my very Smart error Message #1 !");

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"

template <typename T>
static void* MetaThrowHelper(const char* fmt, const char* str) {
  ADD_FAILURE() << "non-specialzer was called";
  return nullptr;
}

template <>
void* MetaThrowHelper<CameraMetadataException>(const char* fmt,
                                               const char* str) {
  ThrowCME(fmt, str);
}

template <>
void* MetaThrowHelper<CiffParserException>(const char* fmt, const char* str) {
  ThrowCPE(fmt, str);
}

template <>
void* MetaThrowHelper<FileIOException>(const char* fmt, const char* str) {
  ThrowFIE(fmt, str);
}

template <>
void* MetaThrowHelper<IOException>(const char* fmt, const char* str) {
  ThrowIOE(fmt, str);
}

template <>
void* MetaThrowHelper<RawDecoderException>(const char* fmt, const char* str) {
  ThrowRDE(fmt, str);
}

template <>
void* MetaThrowHelper<TiffParserException>(const char* fmt, const char* str) {
  ThrowTPE(fmt, str);
}

template <>
void* MetaThrowHelper<FiffParserException>(const char* fmt, const char* str) {
  ThrowFPE(fmt, str);
}

#pragma GCC diagnostic pop

template <class T> class ExceptionsTest : public testing::Test {};

using Classes =
    testing::Types<CameraMetadataException, CiffParserException,
                   FileIOException, IOException, RawDecoderException,
                   TiffParserException, FiffParserException>;

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
  } catch (std::exception& ex) {
    ASSERT_THAT(ex.what(), testing::HasSubstr(msg));
    EXPECT_THAT(ex.what(), testing::StrEq(msg));
  }

  try {
    std::unique_ptr<TypeParam> Exception(new TypeParam(msg));
    throw * Exception.get();
  } catch (std::exception& ex) {
    ASSERT_THAT(ex.what(), testing::HasSubstr(msg));
    EXPECT_THAT(ex.what(), testing::StrEq(msg));
  }

  try {
    std::unique_ptr<TypeParam> ExceptionOne(new TypeParam(msg));
    const std::unique_ptr<const TypeParam> ExceptionTwo(new TypeParam(msg));
    throw * ExceptionTwo.get();
  } catch (std::exception& ex) {
    ASSERT_THAT(ex.what(), testing::HasSubstr(msg));
    EXPECT_THAT(ex.what(), testing::StrEq(msg));
  }

  try {
    const TypeParam ExceptionOne(msg);
    std::unique_ptr<TypeParam> ExceptionTwo(new TypeParam(msg));
    throw * ExceptionTwo.get();
  } catch (std::exception& ex) {
    ASSERT_THAT(ex.what(), testing::HasSubstr(msg));
    EXPECT_THAT(ex.what(), testing::StrEq(msg));
  }
}

TYPED_TEST(ExceptionsTest, ThrowHelperTest) {
  ASSERT_ANY_THROW(MetaThrowHelper<TypeParam>("%s", msg.c_str()));
  EXPECT_THROW(MetaThrowHelper<TypeParam>("%s", msg.c_str()),
               std::runtime_error);
  EXPECT_THROW(MetaThrowHelper<TypeParam>("%s", msg.c_str()), TypeParam);
}

TYPED_TEST(ExceptionsTest, ThrowHelperTestMessage) {
  try {
    MetaThrowHelper<TypeParam>("%s", msg.c_str());
  } catch (std::exception& ex) {
    ASSERT_THAT(ex.what(), testing::HasSubstr(msg));
    EXPECT_THAT(ex.what(), testing::StrEq(msg));
  }
}
