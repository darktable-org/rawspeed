FILE(GLOB SOURCES
  "BlackArea.h"
  "Camera.cpp"
  "Camera.h"
  "CameraMetaData.cpp"
  "CameraMetaData.h"
  "CameraMetadataException.h"
  "CameraSensorInfo.cpp"
  "CameraSensorInfo.h"
  "ColorFilterArray.cpp"
  "ColorFilterArray.h"
)

target_sources(rawspeed PRIVATE
  ${SOURCES}
)
