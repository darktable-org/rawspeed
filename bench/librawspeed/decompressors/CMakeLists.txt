if(HAVE_ZLIB)
  FILE(GLOB RAWSPEED_BENCHS_SOURCES
    "DeflateDecompressorBenchmark.cpp"
  )

  foreach(IN ${RAWSPEED_BENCHS_SOURCES})
    add_rs_bench(${IN})
  endforeach()
endif()

target_link_libraries(DeflateDecompressorBenchmark PRIVATE rawspeed_get_number_of_processor_cores)
