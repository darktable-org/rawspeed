add_subdirectory(identify)

if(BUILD_TESTING)
  add_subdirectory(rstest)
endif()

if(BUILD_BENCHMARKING)
  add_subdirectory(rsbench)
endif()
