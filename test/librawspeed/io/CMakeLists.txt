FILE(GLOB RAWSPEED_TEST_SOURCES
  "EndiannessTest.cpp"
)

foreach(IN ${RAWSPEED_TEST_SOURCES})
  add_rs_test(${IN})
endforeach()
