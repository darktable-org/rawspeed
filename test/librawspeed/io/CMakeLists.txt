FILE(GLOB RAWSPEED_TEST_SOURCES
  "BitPumpJPEGTest.cpp"
  "BitPumpLSBTest.cpp"
  "BitPumpMSB16Test.cpp"
  "BitPumpMSB32Test.cpp"
  "BitPumpMSBTest.cpp"
  "EndiannessTest.cpp"
)

foreach(SRC ${RAWSPEED_TEST_SOURCES})
  add_rs_test("${SRC}")
endforeach()
