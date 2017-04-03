add_executable(rstest rstest.cpp md5.cpp)
set_target_properties(rstest PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(rstest rawspeed_static)
target_include_directories(rstest PUBLIC "${CONFIG_INCLUDE_PATH}")

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

  add_executable(md5 md5.cpp)
  target_compile_definitions(md5
    PRIVATE -DENABLE_SELFTEST
  )
  target_link_libraries(md5 gtest_main)
  add_test(NAME utilities/rstest/md5 COMMAND md5 --gtest_output=xml:${UNITTEST_REPORT_PATH})

  add_test(NAME utilities/rstest COMMAND rstest)
endif()
