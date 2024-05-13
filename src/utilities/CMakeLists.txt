set (DISABLED_WARNING_FLAGS
  "global-constructors"
  "exit-time-destructors"
)

foreach(warning ${DISABLED_WARNING_FLAGS})
  CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wno-${warning})
endforeach()

add_subdirectory(identify)

add_subdirectory(rstest)

if(BUILD_BENCHMARKING)
  add_subdirectory(rsbench)
endif()
