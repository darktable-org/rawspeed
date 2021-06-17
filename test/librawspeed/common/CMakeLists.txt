FILE(GLOB RAWSPEED_TEST_SOURCES
  "ChecksumFileTest.cpp"
  "CommonTest.cpp"
  "MemoryTest.cpp"
  "NORangesSetTest.cpp"
  "PointTest.cpp"
  "RangeTest.cpp"
  "SplineTest.cpp"
)

foreach(SRC ${RAWSPEED_TEST_SOURCES})
  add_rs_test("${SRC}")
endforeach()
