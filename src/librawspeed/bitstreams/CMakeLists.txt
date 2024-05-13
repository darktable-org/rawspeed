rawspeed_add_library(rawspeed_bitstreams OBJECT)
set_target_properties(rawspeed_bitstreams PROPERTIES LINKER_LANGUAGE CXX)

FILE(GLOB SOURCES
  "BitStream.h"
  "BitStreamJPEG.h"
  "BitStreamLSB.h"
  "BitStreamMSB.h"
  "BitStreamMSB16.h"
  "BitStreamMSB32.h"
  "BitStreamPosition.h"
  "BitStreamer.cpp"
  "BitStreamer.h"
  "BitStreamerJPEG.h"
  "BitStreamerLSB.h"
  "BitStreamerMSB.h"
  "BitStreamerMSB16.h"
  "BitStreamerMSB32.h"
  "BitStreams.h"
  "BitVacuumer.h"
  "BitVacuumerJPEG.h"
  "BitVacuumerLSB.h"
  "BitVacuumerMSB.h"
  "BitVacuumerMSB16.h"
  "BitVacuumerMSB32.h"
)

target_sources(rawspeed_bitstreams PRIVATE
  ${SOURCES}
)

target_include_directories(rawspeed_bitstreams PUBLIC "${RAWSPEED_BINARY_DIR}/src")
target_include_directories(rawspeed_bitstreams SYSTEM PUBLIC "${RAWSPEED_SOURCE_DIR}/src/external")
target_include_directories(rawspeed_bitstreams PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")

target_link_libraries(rawspeed PRIVATE rawspeed_bitstreams)
