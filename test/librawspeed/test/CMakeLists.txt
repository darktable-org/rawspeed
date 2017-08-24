FILE(GLOB RAWSPEED_TEST_SOURCES
  "RawSpeed.cpp"
)

if(CMAKE_BUILD_TYPE STREQUAL "COVERAGE")
  # want all the symbols.
  add_library(rawspeed_test SHARED)
else()
  add_library(rawspeed_test STATIC)
endif()

target_sources(rawspeed_test PRIVATE
  ${RAWSPEED_TEST_SOURCES}
)

target_link_libraries(rawspeed_test PUBLIC rawspeed)
target_link_libraries(rawspeed_test PUBLIC gtest gmock_main)

if(WITH_OPENMP AND OPENMP_FOUND AND TARGET OpenMP::OpenMP)
  target_link_libraries(rawspeed_test PUBLIC OpenMP::OpenMP)
endif()

FILE(GLOB RAWSPEED_TESTS_SOURCES
  "../common/CommonTest.cpp"
  "../common/CpuidTest.cpp"
  "../common/MemoryTest.cpp"
  "../common/PointTest.cpp"
  "../io/EndiannessTest.cpp"
  "../metadata/BlackAreaTest.cpp"
  "../metadata/CameraMetaDataTest.cpp"
  "../metadata/CameraSensorInfoTest.cpp"
  "../metadata/CameraTest.cpp"
  "../metadata/ColorFilterArrayTest.cpp"
  "ExceptionsTest.cpp"
)

set (DISABLED_WARNING_FLAGS
  "missing-prototypes"
  "missing-variable-declarations"
  "suggest-attribute=const"
  "suggest-override"
  "used-but-marked-unused"
)

foreach(warning ${DISABLED_WARNING_FLAGS})
  CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wno-${warning})
endforeach()

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

# FIXME: it should be PATH, but then it is escaped, and that breaks gtest
set(UNITTEST_REPORT_PATH ${CMAKE_BINARY_DIR}/unittest-reports/ CACHE STRING "" FORCE)
file(MAKE_DIRECTORY "${UNITTEST_REPORT_PATH}")

foreach(IN ${RAWSPEED_TESTS_SOURCES})
  get_filename_component(TESTNAME ${IN} NAME_WE)
  add_executable(${TESTNAME} ${IN})
  target_link_libraries(${TESTNAME} rawspeed)
  target_link_libraries(${TESTNAME} rawspeed_test)
  # GTEST_ADD_TESTS(${TESTNAME} "" AUTO)
  add_test(NAME ${TESTNAME} COMMAND ${TESTNAME} --gtest_output=xml:${UNITTEST_REPORT_PATH})
  add_dependencies(tests ${TESTNAME})
endforeach()

set(CMAKE_CXX_CLANG_TIDY "${CMAKE_CXX_CLANG_TIDY_SAVE}")
