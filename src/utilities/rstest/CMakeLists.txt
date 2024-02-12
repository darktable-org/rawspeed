if(NOT BUILD_TESTING)
  set_directory_properties(PROPERTIES EXCLUDE_FROM_ALL ON)
endif()

rawspeed_add_executable(rstest rstest.cpp md5.cpp)
target_link_libraries(rstest rawspeed)

target_link_libraries(rstest rawspeed_get_number_of_processor_cores)

if(BUILD_TESTING)
  CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wno-suggest-attribute=const)
  CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wno-suggest-override)
  CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wno-missing-prototypes)

  if(NOT RAWSPEED_SPECIAL_BUILD)
    # should be < 64Kb
    math(EXPR MAX_MEANINGFUL_SIZE 4*1024)
    CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wstack-usage=${MAX_MEANINGFUL_SIZE})
    CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wframe-larger-than=${MAX_MEANINGFUL_SIZE})

    # as small as possible, but 1Mb+ is ok.
    math(EXPR MAX_MEANINGFUL_SIZE 32*1024)
    CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wlarger-than=${MAX_MEANINGFUL_SIZE})
  endif()

  CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wno-c99-extensions)
  CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wno-missing-variable-declarations)
  rawspeed_add_executable(MD5Test md5.cpp MD5Test.cpp)
  target_include_directories(MD5Test PRIVATE "${RAWSPEED_BINARY_DIR}/src")
  target_include_directories(MD5Test PRIVATE "${RAWSPEED_SOURCE_DIR}/src/librawspeed")
  target_link_libraries(MD5Test gtest_main)
  rawspeed_add_test(NAME utilities/rstest/md5 COMMAND MD5Test)
  set_tests_properties(utilities/rstest/md5 PROPERTIES LABELS "unittest")
  add_dependencies(tests MD5Test)

  rawspeed_add_test(NAME utilities/rstest COMMAND rstest
                    WORKING_DIRECTORY "$<TARGET_PROPERTY:rawspeed_get_number_of_processor_cores,BINARY_DIR>")
  set_tests_properties(utilities/rstest PROPERTIES LABELS "dummy")
endif()

if(BUILD_BENCHMARKING)
  rawspeed_add_executable(MD5Benchmark md5.cpp MD5Benchmark.cpp)
  target_link_libraries(MD5Benchmark rawspeed_bench)

  rawspeed_add_test(NAME benchmarks/rstest/MD5Benchmark-Dummy COMMAND MD5Benchmark --help)
  set_tests_properties(benchmarks/rstest/MD5Benchmark-Dummy PROPERTIES LABELS "benchmark;dummy")
  if(WIN32)
    set_tests_properties(benchmarks/rstest/MD5Benchmark-Dummy PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:rawspeed_bench>")
  endif()

  rawspeed_add_test(NAME benchmarks/rstest/MD5Benchmark COMMAND MD5Benchmark --benchmark_min_time=1x)
  set_tests_properties(benchmarks/rstest/MD5Benchmark PROPERTIES LABELS "benchmark")
  set_tests_properties(benchmarks/rstest/MD5Benchmark PROPERTIES ENVIRONMENT "RAWSPEED_BENCHMARK_DRYRUN=1")
  if(WIN32)
    set_tests_properties(benchmarks/rstest/MD5Benchmark PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:rawspeed_bench>")
  endif()

  add_dependencies(benchmarks MD5Benchmark)
endif()

if(RAWSPEED_ENABLE_SAMPLE_BASED_TESTING)
  include(sample-based-testing)
endif()
