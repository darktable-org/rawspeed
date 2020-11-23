FILE(GLOB SOURCES
  "Array2DRef.h"
  "ChecksumFile.cpp"
  "ChecksumFile.h"
  "Common.cpp"
  "Common.h"
  "Cpuid.cpp"
  "DefaultInitAllocatorAdaptor.h"
  "DngOpcodes.cpp"
  "DngOpcodes.h"
  "ErrorLog.cpp"
  "ErrorLog.h"
  "FloatingPoint.h"
  "Memory.cpp"
  "Memory.h"
  "Mutex.h"
  "NORangesSet.h"
  "Optional.h"
  "Point.h"
  "Range.h"
  "RawImage.cpp"
  "RawImage.h"
  "RawImageDataFloat.cpp"
  "RawImageDataU16.cpp"
  "RawspeedException.h"
  "SimpleLUT.h"
  "Spline.h"
  "TableLookUp.cpp"
  "TableLookUp.h"
)

target_sources(rawspeed PRIVATE
  ${SOURCES}
)

# Provide naive implementation of rawspeed_get_number_of_processor_cores().

if(RAWSPEED_COVERAGE_BUILD)
  # want all the symbols.
  rawspeed_add_library(rawspeed_get_number_of_processor_cores SHARED)
else()
  rawspeed_add_library(rawspeed_get_number_of_processor_cores STATIC)
endif()

FILE(GLOB SOURCES
  "GetNumberOfProcessorCores.cpp"
)

target_sources(rawspeed_get_number_of_processor_cores PRIVATE
  ${SOURCES}
)

target_link_libraries(rawspeed_get_number_of_processor_cores PRIVATE rawspeed)
