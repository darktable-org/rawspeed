target_include_directories(rawspeed_fuzz PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

add_subdirectory(decoders)
add_subdirectory(decompressors)
add_subdirectory(fuzz)
add_subdirectory(parsers)
