add_custom_target(benchmarks ALL)

FILE(GLOB RAWSPEED_BENCH_SOURCES
  "../benchmark/Common.cpp"
  "${CMAKE_SOURCE_DIR}/test/librawspeed/test/RawSpeed.cpp"
)

if(CMAKE_BUILD_TYPE STREQUAL "COVERAGE")
  # want all the symbols.
  add_library(rawspeed_benchmark SHARED)
else()
  add_library(rawspeed_benchmark STATIC)
endif()

target_sources(rawspeed_benchmark PRIVATE
  ${RAWSPEED_BENCH_SOURCES}
)

target_link_libraries(rawspeed_benchmark PUBLIC rawspeed)
target_link_libraries(rawspeed_benchmark PUBLIC benchmark)

FILE(GLOB RAWSPEED_BENCHMARKS_SOURCES
  "../interpolators/Cr2sRawInterpolatorBenchmark.cpp"
  "../io/BitStreamBenchmark.cpp"
)

if(HAVE_ZLIB)
  FILE(GLOB RAWSPEED_BENCHMARKS_SOURCES
    ${RAWSPEED_BENCHMARKS_SOURCES}
    "../decompressors/DeflateDecompressorBenchmark.cpp"
  )
endif()

foreach(IN ${RAWSPEED_BENCHMARKS_SOURCES})
  get_filename_component(BENCHNAME ${IN} NAME_WE)
  add_executable(${BENCHNAME} ${IN})
  target_link_libraries(${BENCHNAME} PUBLIC rawspeed)
  target_link_libraries(${BENCHNAME} PUBLIC rawspeed_benchmark)

  add_dependencies(benchmarks ${BENCHNAME})
endforeach()
