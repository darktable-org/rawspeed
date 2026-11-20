FILE(GLOB RAWSPEED_TEST_SOURCES
  "Cr2sRawInterpolatorTest.cpp"
)

foreach(SRC ${RAWSPEED_TEST_SOURCES})
  add_rs_test("${SRC}")
endforeach()
