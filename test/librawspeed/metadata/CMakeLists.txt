FILE(GLOB RAWSPEED_TEST_SOURCES
  "BlackAreaTest.cpp"
  "CameraMetaDataTest.cpp"
  "CameraSensorInfoTest.cpp"
  "CameraTest.cpp"
  "ColorFilterArrayTest.cpp"
)

foreach(SRC ${RAWSPEED_TEST_SOURCES})
  add_rs_test("${SRC}")
endforeach()
