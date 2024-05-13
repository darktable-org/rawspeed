FILE(GLOB RAWSPEED_BENCH_SOURCES
  "BitStreamJPEGUtils.cpp"
  "BitStreamJPEGUtils.h"
)

target_sources(rawspeed_bench PRIVATE
  ${RAWSPEED_BENCH_SOURCES}
)


FILE(GLOB RAWSPEED_BENCHS_SOURCES
  "BitStreamerBenchmark.cpp"
  "BitStreamerJPEGBenchmark.cpp"
  "BitVacuumerBenchmark.cpp"
  "BitVacuumerJPEGBenchmark.cpp"
)

foreach(SRC ${RAWSPEED_BENCHS_SOURCES})
  add_rs_bench("${SRC}")
endforeach()
