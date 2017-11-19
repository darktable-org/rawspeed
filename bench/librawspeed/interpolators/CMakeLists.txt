FILE(GLOB RAWSPEED_BENCHS_SOURCES
  "Cr2sRawInterpolatorBenchmark.cpp"
)

foreach(IN ${RAWSPEED_BENCHS_SOURCES})
  add_rs_bench(${IN})
endforeach()
