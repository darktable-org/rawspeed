FILE(GLOB RAWSPEED_TEST_SOURCES
  "HuffmanCodeTest.cpp"
  "PrefixCodeDecoderTest.cpp"
)

foreach(SRC ${RAWSPEED_TEST_SOURCES})
  add_rs_test("${SRC}")
endforeach()
