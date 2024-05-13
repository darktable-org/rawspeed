rawspeed_add_library(rawspeed_interpolators OBJECT)
set_target_properties(rawspeed_interpolators PROPERTIES LINKER_LANGUAGE CXX)

FILE(GLOB SOURCES
  "Cr2sRawInterpolator.cpp"
  "Cr2sRawInterpolator.h"
)

target_sources(rawspeed_interpolators PRIVATE
  ${SOURCES}
)

target_include_directories(rawspeed_interpolators PUBLIC "${RAWSPEED_BINARY_DIR}/src")
target_include_directories(rawspeed_interpolators SYSTEM PUBLIC "${RAWSPEED_SOURCE_DIR}/src/external")
target_include_directories(rawspeed_interpolators PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")

if(TARGET RawSpeed::OpenMP_CXX)
  target_link_libraries(rawspeed_interpolators PUBLIC RawSpeed::OpenMP_CXX)
endif()

target_link_libraries(rawspeed PRIVATE rawspeed_interpolators)
