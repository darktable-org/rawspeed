FILE(GLOB RAWSPEED_TEST_SOURCES
  "BitStreamerJPEGTest.cpp"
  "BitStreamerLSBTest.cpp"
  "BitStreamerMSB16Test.cpp"
  "BitStreamerMSB32Test.cpp"
  "BitStreamerMSBTest.cpp"
  "BitVacuumerMSBTest.cpp"
  "EndiannessTest.cpp"
)

foreach(SRC ${RAWSPEED_TEST_SOURCES})
  add_rs_test("${SRC}")
endforeach()
