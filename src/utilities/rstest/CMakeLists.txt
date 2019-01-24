if(NOT BUILD_TESTING)
  set_directory_properties(PROPERTIES EXCLUDE_FROM_ALL ON)
endif()

add_executable(rstest rstest.cpp md5.cpp)
target_link_libraries(rstest rawspeed)

if(TARGET RawSpeed::OpenMP_CXX)
  target_link_libraries(rstest RawSpeed::OpenMP_CXX)
endif()

if(BUILD_TESTING)
  CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wno-suggest-attribute=const)
  CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wno-suggest-override)
  CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wno-missing-prototypes)

  if(NOT SPECIAL_BUILD)
    # should be < 64Kb
    math(EXPR MAX_MEANINGFUL_SIZE 4*1024)
    CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wstack-usage=${MAX_MEANINGFUL_SIZE})
    CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wframe-larger-than=${MAX_MEANINGFUL_SIZE})

    # as small as possible, but 1Mb+ is ok.
    math(EXPR MAX_MEANINGFUL_SIZE 32*1024)
    CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wlarger-than=${MAX_MEANINGFUL_SIZE})
  endif()

  set(CMAKE_CXX_CLANG_TIDY_SAVE "${CMAKE_CXX_CLANG_TIDY}")

  unset(CMAKE_CXX_CLANG_TIDY)

  CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wno-c99-extensions)
  CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wno-missing-variable-declarations)
  add_executable(MD5Test md5.cpp MD5Test.cpp)
  target_link_libraries(MD5Test gtest_main)
  add_test(NAME utilities/rstest/md5 COMMAND MD5Test --gtest_output=xml:${UNITTEST_REPORT_PATH})
  add_dependencies(tests MD5Test)

  add_test(NAME utilities/rstest COMMAND rstest)
endif()

if(BUILD_BENCHMARKING)
  add_executable(MD5Benchmark md5.cpp MD5Benchmark.cpp)
  target_link_libraries(MD5Benchmark benchmark)

  add_dependencies(benchmarks MD5Benchmark)
endif()

if(RAWSPEED_ENABLE_SAMPLE_BASED_TESTING)
  include(sample-based-testing)
endif()
