rawspeed_add_library(rawspeed_metadata OBJECT)
set_target_properties(rawspeed_metadata PROPERTIES LINKER_LANGUAGE CXX)

FILE(GLOB SOURCES
  "BlackArea.h"
  "Camera.cpp"
  "Camera.h"
  "CameraMetaData.cpp"
  "CameraMetaData.h"
  "CameraMetadataException.cpp"
  "CameraMetadataException.h"
  "CameraSensorInfo.cpp"
  "CameraSensorInfo.h"
  "ColorFilterArray.cpp"
  "ColorFilterArray.h"
)

target_sources(rawspeed_metadata PRIVATE
  ${SOURCES}
)

target_include_directories(rawspeed_metadata PUBLIC "${RAWSPEED_BINARY_DIR}/src")
target_include_directories(rawspeed_metadata PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")

if(WITH_PUGIXML AND TARGET Pugixml::Pugixml)
  target_link_libraries(rawspeed_metadata PUBLIC Pugixml::Pugixml)
endif()

target_link_libraries(rawspeed PRIVATE rawspeed_metadata)
