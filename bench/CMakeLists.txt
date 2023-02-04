set (DISABLED_WARNING_FLAGS
  "global-constructors"
)

foreach(warning ${DISABLED_WARNING_FLAGS})
  CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wno-${warning})
endforeach()

add_subdirectory(librawspeed)
