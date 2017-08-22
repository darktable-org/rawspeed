add_subdirectory(common)
add_subdirectory(metadata)
add_subdirectory(io)
add_subdirectory(tiff)
add_subdirectory(parsers)
add_subdirectory(decompressors)
add_subdirectory(interpolators)
add_subdirectory(decoders)

target_include_directories(rawspeed PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

if(BUILD_TESTING)
  add_subdirectory(test)
endif()

if(BUILD_BENCHMARKING)
  add_subdirectory(benchmark)
endif()
