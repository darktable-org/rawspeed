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

#include "common/RawspeedException.h"         // for RawspeedException
#include "decoders/RawDecoderException.h"     // for RawDecoderException (p...
#include "io/FileIOException.h"               // for FileIOException (ptr o...
#include "io/IOException.h"                   // for IOException (ptr only)
#include "metadata/CameraMetadataException.h" // for CameraMetadataExceptio...
#include "parsers/CiffParserException.h"      // for CiffParserException (p...
#include "parsers/FiffParserException.h"      // for FiffParserException (p...
#include "parsers/RawParserException.h"       // for ThrowRPE, RawParserEx...
#include "parsers/TiffParserException.h"      // for ThrowTPE, TiffParserEx...
#include "parsers/X3fParserException.h"       // for ThrowXPE, X3fParserExc...
#include <exception>                          // IWYU pragma: keep
#include <gmock/gmock.h>                      // for MakePredicateFormatter...
#include <gtest/gtest.h>                      // for Message, TestPartResult
#include <memory>                             // for unique_ptr
#include <stdexcept>                          // for runtime_error
#include <string>                             // for string
// IWYU pragma: no_include <bits/exception.h>

using std::unique_ptr;
using rawspeed::RawspeedException;
using rawspeed::CameraMetadataException;
using rawspeed::CiffParserException;
using rawspeed::FileIOException;
using rawspeed::IOException;
using rawspeed::RawDecoderException;
using rawspeed::TiffParserException;
using rawspeed::FiffParserException;
using rawspeed::RawParserException;
using rawspeed::X3fParserException;

namespace rawspeed_test {

static const std::string msg("my very Smart error Message #1 !");

#define FMT "%s"

template <typename T> static void* MetaHelper(const char* str) {
  ADD_FAILURE() << "non-specialzer was called";
  return nullptr;
}

template <> void* MetaHelper<RawspeedException>(const char* str) {
  ThrowRSE(FMT, str);
}

template <> void* MetaHelper<CameraMetadataException>(const char* str) {
  ThrowCME(FMT, str);
}

template <> void* MetaHelper<CiffParserException>(const char* str) {
  ThrowCPE(FMT, str);
}

template <> void* MetaHelper<FileIOException>(const char* str) {
  ThrowFIE(FMT, str);
}

template <> void* MetaHelper<IOException>(const char* str) {
  ThrowIOE(FMT, str);
}

template <> void* MetaHelper<RawDecoderException>(const char* str) {
  ThrowRDE(FMT, str);
}

template <> void* MetaHelper<RawParserException>(const char* str) {
  ThrowRPE(FMT, str);
}

template <> void* MetaHelper<TiffParserException>(const char* str) {
  ThrowTPE(FMT, str);
}

template <> void* MetaHelper<FiffParserException>(const char* str) {
  ThrowFPE(FMT, str);
}

template <> void* MetaHelper<X3fParserException>(const char* str) {
  ThrowXPE(FMT, str);
}

template <class T> class ExceptionsTest : public testing::Test {};

using Classes =
    testing::Types<RawspeedException, CameraMetadataException,
                   CiffParserException, FileIOException, IOException,
                   RawDecoderException, TiffParserException,
                   FiffParserException, RawParserException, X3fParserException>;

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
  EXPECT_THROW(throw TypeParam(msg), RawspeedException);
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
      RawspeedException);
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
  }

  try {
    std::unique_ptr<TypeParam> Exception(new TypeParam(msg));
    throw * Exception.get();
  } catch (std::exception& ex) {
    ASSERT_THAT(ex.what(), testing::HasSubstr(msg));
  }

  try {
    std::unique_ptr<TypeParam> ExceptionOne(new TypeParam(msg));
    const std::unique_ptr<const TypeParam> ExceptionTwo(new TypeParam(msg));
    throw * ExceptionTwo.get();
  } catch (std::exception& ex) {
    ASSERT_THAT(ex.what(), testing::HasSubstr(msg));
  }

  try {
    const TypeParam ExceptionOne(msg);
    std::unique_ptr<TypeParam> ExceptionTwo(new TypeParam(msg));
    throw * ExceptionTwo.get();
  } catch (std::exception& ex) {
    ASSERT_THAT(ex.what(), testing::HasSubstr(msg));
  }
}

TYPED_TEST(ExceptionsTest, ThrowHelperTest) {
  ASSERT_ANY_THROW(MetaHelper<TypeParam>(msg.c_str()));
  EXPECT_THROW(MetaHelper<TypeParam>(msg.c_str()), std::runtime_error);
  EXPECT_THROW(MetaHelper<TypeParam>(msg.c_str()), RawspeedException);
  EXPECT_THROW(MetaHelper<TypeParam>(msg.c_str()), TypeParam);
}

TYPED_TEST(ExceptionsTest, ThrowHelperTestMessage) {
  try {
    MetaHelper<TypeParam>(msg.c_str());
  } catch (std::exception& ex) {
    ASSERT_THAT(ex.what(), testing::HasSubstr(msg));
  }
}

} // namespace rawspeed_test
