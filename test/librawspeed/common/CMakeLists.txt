FILE(GLOB RAWSPEED_TEST_SOURCES
  "BayerPhaseTest.cpp"
  "ChecksumFileTest.cpp"
  "CommonTest.cpp"
  "CpuidTest.cpp"
  "SplineTest.cpp"
)

foreach(SRC ${RAWSPEED_TEST_SOURCES})
  add_rs_test("${SRC}")
endforeach()
