if(HAVE_ZLIB)
  FILE(GLOB RAWSPEED_BENCHS_SOURCES
    "DeflateDecompressorBenchmark.cpp"
  )

  foreach(SRC ${RAWSPEED_BENCHS_SOURCES})
    add_rs_bench("${SRC}")
  endforeach()

  target_link_libraries(DeflateDecompressorBenchmark PRIVATE rawspeed_get_number_of_processor_cores)
  target_link_libraries(DeflateDecompressorBenchmark PRIVATE ZLIB::ZLIB)
endif()
