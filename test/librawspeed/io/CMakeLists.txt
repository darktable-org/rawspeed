FILE(GLOB RAWSPEED_TEST_SOURCES
  "BitPumpLSBTest.cpp"
  "EndiannessTest.cpp"
)

foreach(IN ${RAWSPEED_TEST_SOURCES})
  add_rs_test(${IN})
endforeach()
