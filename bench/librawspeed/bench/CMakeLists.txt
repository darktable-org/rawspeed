FILE(GLOB RAWSPEED_BENCH_SOURCES
  "${CMAKE_SOURCE_DIR}/test/librawspeed/test/RawSpeed.cpp"
  "Common.cpp"
  "Common.h"
)

target_sources(rawspeed_bench PRIVATE
  ${RAWSPEED_BENCH_SOURCES}
)
