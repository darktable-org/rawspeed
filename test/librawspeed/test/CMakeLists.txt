FILE(GLOB RAWSPEED_TEST_SOURCES
  "RawSpeed.cpp"
)

target_sources(rawspeed_test PRIVATE
  ${RAWSPEED_TEST_SOURCES}
)

FILE(GLOB RAWSPEED_TESTS_SOURCES
  "ExceptionsTest.cpp"
)

foreach(SRC ${RAWSPEED_TEST_SOURCES})
  add_rs_test("${SRC}")
endforeach()
