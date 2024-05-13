rawspeed_add_library(rawspeed_tiff OBJECT)
set_target_properties(rawspeed_tiff PROPERTIES LINKER_LANGUAGE CXX)

FILE(GLOB SOURCES
  "TiffEntry.cpp"
  "TiffEntry.h"
  "TiffIFD.cpp"
  "TiffIFD.h"
  "TiffTag.h"
  "CiffEntry.cpp"
  "CiffEntry.h"
  "CiffIFD.cpp"
  "CiffIFD.h"
  "CiffTag.h"
)

target_sources(rawspeed_tiff PRIVATE
  ${SOURCES}
)

target_include_directories(rawspeed_tiff PUBLIC "${RAWSPEED_BINARY_DIR}/src")
target_include_directories(rawspeed_tiff SYSTEM PUBLIC "${RAWSPEED_SOURCE_DIR}/src/external")
target_include_directories(rawspeed_tiff PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")

target_link_libraries(rawspeed PRIVATE rawspeed_tiff)
