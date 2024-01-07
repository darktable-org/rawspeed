FILE(GLOB RAWSPEED_TEST_SOURCES
  "NORangesSetTest.cpp"
  "PointTest.cpp"
  "RangeTest.cpp"
  "VariableLengthLoadTest.cpp"
)

foreach(SRC ${RAWSPEED_TEST_SOURCES})
  add_rs_test("${SRC}")
endforeach()
