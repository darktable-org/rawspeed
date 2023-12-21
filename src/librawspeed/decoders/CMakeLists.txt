rawspeed_add_library(rawspeed_decoders OBJECT)
set_target_properties(rawspeed_decoders PROPERTIES LINKER_LANGUAGE CXX)

FILE(GLOB SOURCES
  "AbstractTiffDecoder.cpp"
  "AbstractTiffDecoder.h"
  "ArwDecoder.cpp"
  "ArwDecoder.h"
  "Cr2Decoder.cpp"
  "Cr2Decoder.h"
  "CrwDecoder.cpp"
  "CrwDecoder.h"
  "DcrDecoder.cpp"
  "DcrDecoder.h"
  "DcsDecoder.cpp"
  "DcsDecoder.h"
  "DngDecoder.cpp"
  "DngDecoder.h"
  "ErfDecoder.cpp"
  "ErfDecoder.h"
  "IiqDecoder.cpp"
  "IiqDecoder.h"
  "KdcDecoder.cpp"
  "KdcDecoder.h"
  "MefDecoder.cpp"
  "MefDecoder.h"
  "MosDecoder.cpp"
  "MosDecoder.h"
  "MrwDecoder.cpp"
  "MrwDecoder.h"
  "NakedDecoder.cpp"
  "NakedDecoder.h"
  "NefDecoder.cpp"
  "NefDecoder.h"
  "OrfDecoder.cpp"
  "OrfDecoder.h"
  "PefDecoder.cpp"
  "PefDecoder.h"
  "RafDecoder.cpp"
  "RafDecoder.h"
  "RawDecoder.cpp"
  "RawDecoder.h"
  "RawDecoderException.cpp"
  "RawDecoderException.h"
  "Rw2Decoder.cpp"
  "Rw2Decoder.h"
  "SimpleTiffDecoder.cpp"
  "SimpleTiffDecoder.h"
  "SrwDecoder.cpp"
  "SrwDecoder.h"
  "StiDecoder.cpp"
  "StiDecoder.h"
  "ThreefrDecoder.cpp"
  "ThreefrDecoder.h"
)

target_sources(rawspeed_decoders PRIVATE
  ${SOURCES}
)

target_include_directories(rawspeed_decoders PUBLIC "${RAWSPEED_BINARY_DIR}/src")
target_include_directories(rawspeed_decoders SYSTEM PUBLIC "${RAWSPEED_SOURCE_DIR}/src/external")
target_include_directories(rawspeed_decoders PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")

if(TARGET RawSpeed::OpenMP_CXX)
  target_link_libraries(rawspeed_decoders PUBLIC RawSpeed::OpenMP_CXX)
endif()

target_link_libraries(rawspeed PRIVATE rawspeed_decoders)
