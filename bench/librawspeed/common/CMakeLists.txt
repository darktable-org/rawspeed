FILE(GLOB RAWSPEED_BENCHS_SOURCES
  "CommonBenchmark.cpp"
)

foreach(SRC ${RAWSPEED_BENCHS_SOURCES})
  add_rs_bench("${SRC}")
endforeach()
