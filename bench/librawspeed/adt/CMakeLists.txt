FILE(GLOB RAWSPEED_BENCHS_SOURCES
  "CoalescingOutputIteratorBenchmark.cpp"
  "DefaultInitAllocatorAdaptorBenchmark.cpp"
  "VariableLengthLoadBenchmark.cpp"
)

foreach(SRC ${RAWSPEED_BENCHS_SOURCES})
  add_rs_bench("${SRC}")
endforeach()
