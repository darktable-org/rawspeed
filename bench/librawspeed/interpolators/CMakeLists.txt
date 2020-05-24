FILE(GLOB RAWSPEED_BENCHS_SOURCES
  "Cr2sRawInterpolatorBenchmark.cpp"
)

foreach(SRC ${RAWSPEED_BENCHS_SOURCES})
  add_rs_bench("${SRC}")
endforeach()

target_link_libraries(Cr2sRawInterpolatorBenchmark PRIVATE rawspeed_get_number_of_processor_cores)
