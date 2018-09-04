FILE(GLOB SOURCES
  "ChecksumFile.cpp"
  "ChecksumFile.h"
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
  "NORangesSet.h"
  "Point.h"
  "Range.h"
  "RawImage.cpp"
  "RawImage.h"
  "RawImageDataFloat.cpp"
  "RawImageDataU16.cpp"
  "RawspeedException.h"
  "Spline.h"
  "TableLookUp.cpp"
  "TableLookUp.h"
  "Threading.h"
)

target_sources(rawspeed PRIVATE
  ${SOURCES}
)
