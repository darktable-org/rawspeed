FILE(GLOB RAWSPEED_TEST_SOURCES
  "BlackAreaTest.cpp"
  "CameraMetaDataTest.cpp"
  "CameraSensorInfoTest.cpp"
  "CameraTest.cpp"
  "ColorFilterArrayTest.cpp"
)

foreach(IN ${RAWSPEED_TEST_SOURCES})
  add_rs_test(${IN})
endforeach()
