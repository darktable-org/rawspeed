FILE(GLOB COMMON_SOURCES
  "Common.cpp"
  "Common.h"
  "Cpuid.cpp"
  "Memory.cpp"
  "Memory.h"
  "Point.h"
  "RawImage.cpp"
  "RawImage.h"
  "RawImageDataFloat.cpp"
  "RawImageDataU16.cpp"
  "DngOpcodes.cpp"
  "DngOpcodes.h"
  "RawspeedException.h"
)

set(RAWSPEED_SOURCES "${RAWSPEED_SOURCES};${COMMON_SOURCES}" PARENT_SCOPE)
