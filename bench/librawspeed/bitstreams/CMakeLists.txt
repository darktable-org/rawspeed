FILE(GLOB RAWSPEED_BENCHS_SOURCES
  "BitStreamerJPEGBenchmark.cpp"
  "BitStreamerBenchmark.cpp"
  "BitVacuumerBenchmark.cpp"
)

foreach(SRC ${RAWSPEED_BENCHS_SOURCES})
  add_rs_bench("${SRC}")
endforeach()
