FILE(GLOB COMMON_SOURCES
  "Common.cpp"
  "Common.h"
  "Cpuid.cpp"
  "DngOpcodes.cpp"
  "DngOpcodes.h"
  "ErrorLog.cpp"
  "ErrorLog.h"
  "Memory.cpp"
  "Memory.h"
  "Mutex.h"
  "Point.h"
  "RawImage.cpp"
  "RawImage.h"
  "RawImageDataFloat.cpp"
  "RawImageDataU16.cpp"
  "RawspeedException.h"
)

set(RAWSPEED_SOURCES "${RAWSPEED_SOURCES};${COMMON_SOURCES}" PARENT_SCOPE)
