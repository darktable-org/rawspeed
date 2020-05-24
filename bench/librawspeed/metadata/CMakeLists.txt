if(NOT HAVE_PUGIXML)
  return()
endif()

FILE(GLOB RAWSPEED_BENCHS_SOURCES
  "CameraMetaDataBenchmark.cpp"
)

foreach(SRC ${RAWSPEED_BENCHS_SOURCES})
  add_rs_bench("${SRC}")
endforeach()
