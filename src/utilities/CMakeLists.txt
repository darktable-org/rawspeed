add_subdirectory(identify)

add_subdirectory(rstest)

if(BUILD_BENCHMARKING)
  add_subdirectory(rsbench)
endif()
