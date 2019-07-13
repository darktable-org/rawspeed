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

#include "common/Memory.h" // for alignedMallocArray, alignedFree, alignedM...
#include "common/Common.h" // for uint8_t, int64_t, int32_t, int16_t, uint32
#include <cstddef>         // for size_t
#include <cstdint>         // for SIZE_MAX, uintptr_t
#include <cstdlib>         // for exit
#include <gtest/gtest.h>   // for Message, TestPartResult, TestPartResult::...
#include <memory>          // for unique_ptr

using rawspeed::alignedFree;
using rawspeed::alignedFreeConstPtr;
using rawspeed::alignedMalloc;
using rawspeed::alignedMallocArray;
using rawspeed::uint32;
using std::unique_ptr;

namespace rawspeed_test {

static constexpr const size_t alloc_alignment = 16;

template <typename T> class AlignedMallocTest : public testing::Test {
public:
  static constexpr const size_t alloc_cnt = 16;
  static constexpr const size_t alloc_sizeof = sizeof(T);
  static constexpr const size_t alloc_size = alloc_cnt * alloc_sizeof;

  inline void TheTest(T* ptr) {
    ASSERT_TRUE(((uintptr_t)ptr % alloc_alignment) == 0);
    ptr[0] = 0;
    ptr[1] = 8;
    ptr[2] = 16;
    ptr[3] = 24;
    ptr[4] = 32;
    ptr[5] = 40;
    ptr[6] = 48;
    ptr[7] = 56;
    ptr[8] = 64;
    ptr[9] = 72;
    ptr[10] = 80;
    ptr[11] = 88;
    ptr[12] = 96;
    ptr[13] = 104;
    ptr[14] = 112;
    ptr[15] = 120;

    ASSERT_EQ((int64_t)ptr[0] + ptr[1] + ptr[2] + ptr[3] + ptr[4] + ptr[5] +
                  ptr[6] + ptr[7] + ptr[8] + ptr[9] + ptr[10] + ptr[11] +
                  ptr[12] + ptr[13] + ptr[14] + ptr[15],
              960UL);
  }
};

template <typename T>
class AlignedMallocDeathTest : public AlignedMallocTest<T> {};

using Classes =
    testing::Types<int, unsigned int, int8_t, uint8_t, int16_t, uint16_t,
                   int32_t, uint32, int64_t, uint64_t, float, double>;

TYPED_TEST_CASE(AlignedMallocTest, Classes);

TYPED_TEST_CASE(AlignedMallocDeathTest, Classes);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
TYPED_TEST(AlignedMallocTest, BasicTest) {
  ASSERT_NO_THROW({
    TypeParam* ptr =
        (TypeParam*)alignedMalloc(this->alloc_size, alloc_alignment);
    this->TheTest(ptr);
    alignedFree(ptr);
  });
  ASSERT_NO_THROW({
    const TypeParam* forFree = nullptr;
    {
      TypeParam* ptr =
          (TypeParam*)alignedMalloc(this->alloc_size, alloc_alignment);
      this->TheTest(ptr);
      forFree = const_cast<const TypeParam*>(ptr);
      ptr = nullptr;
    }
    alignedFreeConstPtr(forFree);
  });
}

TYPED_TEST(AlignedMallocTest, UniquePtrTest) {
  unique_ptr<TypeParam[], decltype(&alignedFree)> ptr(
      (TypeParam*)alignedMalloc(this->alloc_size, alloc_alignment),
      alignedFree);
  this->TheTest(&(ptr[0]));
}

TYPED_TEST(AlignedMallocDeathTest, AlignedMallocAssertions) {
#ifndef NDEBUG
  ASSERT_DEATH(
      {
        TypeParam* ptr = (TypeParam*)alignedMalloc(this->alloc_size, 3);
        this->TheTest(ptr);
        alignedFree(ptr);
      },
      "isPowerOfTwo");
  ASSERT_DEATH(
      {
        TypeParam* ptr =
            (TypeParam*)alignedMalloc(this->alloc_size, sizeof(void*) / 2);
        this->TheTest(ptr);
        alignedFree(ptr);
      },
      "isAligned\\(alignment\\, sizeof\\(void\\*\\)\\)");
  ASSERT_DEATH(
      {
        TypeParam* ptr =
            (TypeParam*)alignedMalloc(1 + alloc_alignment, alloc_alignment);
        this->TheTest(ptr);
        alignedFree(ptr);
      },
      "isAligned\\(size\\, alignment\\)");
#endif
}

#pragma GCC diagnostic pop

TEST(AlignedMallocDeathTest, AlignedFreeHandlesNullptr) {
  ASSERT_EXIT(
      {
        alignedFree(nullptr);
        exit(0);
      },
      ::testing::ExitedWithCode(0), "");
  ASSERT_EXIT(
      {
        alignedFreeConstPtr(nullptr);
        exit(0);
      },
      ::testing::ExitedWithCode(0), "");
}

TYPED_TEST(AlignedMallocTest, TemplateTest) {
  ASSERT_NO_THROW({
    TypeParam* ptr =
        (alignedMalloc<TypeParam, alloc_alignment>(this->alloc_size));
    this->TheTest(ptr);
    alignedFree(ptr);
  });
}

TYPED_TEST(AlignedMallocTest, TemplatUniquePtrTest) {
  unique_ptr<TypeParam[], decltype(&alignedFree)> ptr(
      alignedMalloc<TypeParam, alloc_alignment>(this->alloc_size), alignedFree);
  this->TheTest(&(ptr[0]));
}

TYPED_TEST(AlignedMallocTest, TemplateArrayTest) {
  ASSERT_NO_THROW({
    TypeParam* ptr = (alignedMallocArray<TypeParam, alloc_alignment>(
        this->alloc_cnt, this->alloc_sizeof));
    this->TheTest(ptr);
    alignedFree(ptr);
  });
}

TYPED_TEST(AlignedMallocTest, TemplateArrayHandlesOverflowTest) {
  if (this->alloc_sizeof == 1)
    return;
  ASSERT_NO_THROW({
    static const size_t nmemb = 1 + (SIZE_MAX / this->alloc_sizeof);
    TypeParam* ptr = (alignedMallocArray<TypeParam, alloc_alignment>(
        nmemb, this->alloc_sizeof));
    ASSERT_EQ(ptr, nullptr);
  });
}

TYPED_TEST(AlignedMallocTest, TemplateUniquePtrArrayTest) {
  unique_ptr<TypeParam[], decltype(&alignedFree)> ptr(
      alignedMallocArray<TypeParam, alloc_alignment>(this->alloc_cnt,
                                                     this->alloc_sizeof),
      alignedFree);
  this->TheTest(&(ptr[0]));
}

TYPED_TEST(AlignedMallocDeathTest, TemplateArrayAssertions) {
#ifndef NDEBUG
  // unlike TemplateArrayRoundUp, should fail
  ASSERT_DEATH(
      {
        TypeParam* ptr = (alignedMallocArray<TypeParam, alloc_alignment>(
            1, 1 + sizeof(TypeParam)));
        alignedFree(ptr);
      },
      "isAligned\\(size\\, alignment\\)");
#endif
}

TYPED_TEST(AlignedMallocTest, TemplateArrayRoundUp) {
  // unlike TemplateArrayAssertions, should NOT fail
  ASSERT_NO_THROW({
    TypeParam* ptr = (alignedMallocArray<TypeParam, alloc_alignment, true>(
        1, 1 + sizeof(TypeParam)));
    alignedFree(ptr);
  });
}

TYPED_TEST(AlignedMallocTest, TemplateArraySizeTest) {
  ASSERT_NO_THROW({
    TypeParam* ptr = (alignedMallocArray<TypeParam, alloc_alignment, TypeParam>(
        this->alloc_cnt));
    this->TheTest(ptr);
    alignedFree(ptr);
  });
}

TYPED_TEST(AlignedMallocTest, TemplateUniquePtrArraySizeTest) {
  unique_ptr<TypeParam[], decltype(&alignedFree)> ptr(
      alignedMallocArray<TypeParam, alloc_alignment, TypeParam>(
          this->alloc_cnt),
      alignedFree);
  this->TheTest(&(ptr[0]));
}

TEST(AlignedMallocDeathTest, TemplateArraySizeAssertions) {
#ifndef NDEBUG
  // unlike TemplateArraySizeRoundUp, should fail
  ASSERT_DEATH(
      {
        uint8_t* ptr =
            (alignedMallocArray<uint8_t, alloc_alignment, uint8_t>(1));
        alignedFree(ptr);
      },
      "isAligned\\(size\\, alignment\\)");
#endif
}

TEST(AlignedMallocTest, TemplateArraySizeRoundUp) {
  // unlike TemplateArraySizeAssertions, should NOT fail
  ASSERT_NO_THROW({
    uint8_t* ptr =
        (alignedMallocArray<uint8_t, alloc_alignment, uint8_t, true>(1));
    alignedFree(ptr);
  });
}

TYPED_TEST(AlignedMallocTest, TemplateArraySizeRoundUpTest) {
  // unlike TemplateArraySizeAssertions, should NOT fail
  ASSERT_NO_THROW({
    TypeParam* ptr =
        (alignedMallocArray<TypeParam, alloc_alignment, TypeParam, true>(1));
    alignedFree(ptr);
  });
}

} // namespace rawspeed_test
