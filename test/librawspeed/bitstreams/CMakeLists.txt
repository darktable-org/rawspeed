FILE(GLOB RAWSPEED_TEST_SOURCES
  "BitStreamerJPEGTest.cpp"
  "BitStreamerLSBTest.cpp"
  "BitStreamerMSB16Test.cpp"
  "BitStreamerMSB32Test.cpp"
  "BitStreamerMSBTest.cpp"
  "BitVacuumerJPEGTest.cpp"
  "BitVacuumerLSBTest.cpp"
  "BitVacuumerMSB16Test.cpp"
  "BitVacuumerMSB32Test.cpp"
  "BitVacuumerMSBTest.cpp"
)

foreach(SRC ${RAWSPEED_TEST_SOURCES})
  add_rs_test("${SRC}")
endforeach()
