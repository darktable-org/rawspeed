FILE(GLOB RAWSPEED_TEST_SOURCES
  "EndiannessTest.cpp"
)

foreach(SRC ${RAWSPEED_TEST_SOURCES})
  add_rs_test("${SRC}")
endforeach()
