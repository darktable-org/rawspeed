add_custom_target(benchmarks ALL)

FILE(GLOB RAWSPEED_BENCH_SOURCES
  "../benchmark/Common.cpp"
  "../test/RawSpeed.cpp"
)

if(CMAKE_BUILD_TYPE STREQUAL "COVERAGE")
  # want all the symbols.
  add_library(rawspeed_benchmark SHARED $<TARGET_OBJECTS:rawspeed> ${RAWSPEED_BENCH_SOURCES})
else()
  add_library(rawspeed_benchmark STATIC $<TARGET_OBJECTS:rawspeed> ${RAWSPEED_BENCH_SOURCES})
endif()

target_link_libraries(rawspeed_benchmark PUBLIC ${RAWSPEED_LIBS})
target_compile_definitions(rawspeed_benchmark PUBLIC "$<TARGET_PROPERTY:rawspeed,COMPILE_DEFINITIONS>")
target_include_directories(rawspeed_benchmark PUBLIC "$<TARGET_PROPERTY:rawspeed,INCLUDE_DIRECTORIES>")
target_include_directories(rawspeed_benchmark SYSTEM PUBLIC "$<TARGET_PROPERTY:rawspeed,INTERFACE_SYSTEM_INCLUDE_DIRECTORIES>")
add_dependencies(rawspeed_benchmark rawspeed)

target_link_libraries(rawspeed_benchmark PUBLIC benchmark)

if(WITH_OPENMP AND OPENMP_FOUND)
  target_link_libraries(rawspeed_benchmark INTERFACE ${OpenMP_CXX_FLAGS})
endif()

FILE(GLOB RAWSPEED_BENCHMARKS_SOURCES
  "../interpolators/Cr2sRawInterpolatorBenchmark.cpp"
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
  target_link_libraries(${BENCHNAME} rawspeed_benchmark)

  add_dependencies(benchmarks ${BENCHNAME})
endforeach()
