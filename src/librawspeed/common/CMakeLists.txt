rawspeed_add_library(rawspeed_common OBJECT)
set_target_properties(rawspeed_common PROPERTIES LINKER_LANGUAGE CXX)

FILE(GLOB SOURCES
  "BayerPhase.h"
  "ChecksumFile.cpp"
  "ChecksumFile.h"
  "Common.cpp"
  "Common.h"
  "CpuFeatures.cpp"
  "CpuFeatures.h"
  "DngOpcodes.cpp"
  "DngOpcodes.h"
  "ErrorLog.cpp"
  "ErrorLog.h"
  "FloatingPoint.h"
  "GetNumberOfProcessorCores.cpp"
  "RawImage.cpp"
  "RawImage.h"
  "RawImageDataFloat.cpp"
  "RawImageDataU16.cpp"
  "RawspeedException.cpp"
  "RawspeedException.h"
  "SimpleLUT.h"
  "Spline.h"
  "TableLookUp.cpp"
  "TableLookUp.h"
  "XTransPhase.h"
)

target_sources(rawspeed_common PRIVATE
  ${SOURCES}
)

target_include_directories(rawspeed_common PUBLIC "${RAWSPEED_BINARY_DIR}/src")
target_include_directories(rawspeed_common SYSTEM PUBLIC "${RAWSPEED_SOURCE_DIR}/src/external")
target_include_directories(rawspeed_common PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")

if(TARGET RawSpeed::OpenMP_CXX)
  target_link_libraries(rawspeed_common PUBLIC RawSpeed::OpenMP_CXX)
endif()

target_link_libraries(rawspeed PRIVATE rawspeed_common)

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
