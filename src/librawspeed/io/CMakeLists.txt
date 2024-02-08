rawspeed_add_library(rawspeed_io OBJECT)
set_target_properties(rawspeed_io PROPERTIES LINKER_LANGUAGE CXX)

FILE(GLOB SOURCES
  "Buffer.h"
  "ByteStream.h"
  "Endianness.h"
  "FileIO.h"
  "FileIOException.cpp"
  "FileIOException.h"
  "FileReader.cpp"
  "FileReader.h"
  "FileWriter.cpp"
  "FileWriter.h"
  "IOException.cpp"
  "IOException.h"
)

target_sources(rawspeed_io PRIVATE
  ${SOURCES}
)

target_include_directories(rawspeed_io PUBLIC "${RAWSPEED_BINARY_DIR}/src")
target_include_directories(rawspeed_io SYSTEM PUBLIC "${RAWSPEED_SOURCE_DIR}/src/external")
target_include_directories(rawspeed_io PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")

target_link_libraries(rawspeed PRIVATE rawspeed_io)
