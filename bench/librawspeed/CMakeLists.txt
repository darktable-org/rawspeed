if(CMAKE_BUILD_TYPE STREQUAL "COVERAGE")
  # want all the symbols.
  add_library(rawspeed_bench SHARED)
else()
  add_library(rawspeed_bench STATIC)
endif()

target_link_libraries(rawspeed_bench PUBLIC rawspeed)
target_link_libraries(rawspeed_bench PUBLIC benchmark)
target_include_directories(rawspeed_bench PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

function(add_rs_bench src)
  get_filename_component(BENCHNAME ${src} NAME_WE)
  add_executable(${BENCHNAME} ${src})
  target_link_libraries(${BENCHNAME} PUBLIC rawspeed)
  target_link_libraries(${BENCHNAME} PUBLIC rawspeed_bench)

  add_dependencies(benchmarks ${BENCHNAME})
endfunction()

add_subdirectory(bench)
add_subdirectory(common)
add_subdirectory(decompressors)
add_subdirectory(interpolators)
add_subdirectory(io)
add_subdirectory(metadata)
