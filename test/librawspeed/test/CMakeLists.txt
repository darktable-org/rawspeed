FILE(GLOB RAWSPEED_TEST_SOURCES
  "RawSpeed.cpp"
)

target_sources(rawspeed_test PRIVATE
  ${RAWSPEED_TEST_SOURCES}
)

FILE(GLOB RAWSPEED_TESTS_SOURCES
  "ExceptionsTest.cpp"
)

foreach(IN ${RAWSPEED_TESTS_SOURCES})
  add_rs_test(${IN})
endforeach()
