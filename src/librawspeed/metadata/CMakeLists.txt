FILE(GLOB METADATA_SOURCES
  "BlackArea.cpp"
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

set(RAWSPEED_SOURCES "${RAWSPEED_SOURCES};${METADATA_SOURCES}" PARENT_SCOPE)
