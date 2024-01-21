FILE(GLOB RAWSPEED_BENCHS_SOURCES
  "BitPumpJPEGBenchmark.cpp"
  "BitStreamBenchmark.cpp"
)

foreach(SRC ${RAWSPEED_BENCHS_SOURCES})
  add_rs_bench("${SRC}")
endforeach()
