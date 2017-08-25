FILE(GLOB RAWSPEED_TEST_SOURCES
  "CommonTest.cpp"
  "CpuidTest.cpp"
  "MemoryTest.cpp"
  "NORangesSetTest.cpp"
  "PointTest.cpp"
  "RangeTest.cpp"
)

foreach(IN ${RAWSPEED_TEST_SOURCES})
  add_rs_test(${IN})
endforeach()
