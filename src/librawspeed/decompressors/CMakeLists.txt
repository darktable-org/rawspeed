rawspeed_add_library(rawspeed_decompressors OBJECT)
set_target_properties(rawspeed_decompressors PROPERTIES LINKER_LANGUAGE CXX)

FILE(GLOB SOURCES
  "AbstractDecompressor.h"
  "AbstractDngDecompressor.cpp"
  "AbstractDngDecompressor.h"
  "AbstractLJpegDecoder.cpp"
  "AbstractLJpegDecoder.h"
  "AbstractSamsungDecompressor.h"
  "Cr2Decompressor.cpp"
  "Cr2Decompressor.h"
  "Cr2DecompressorImpl.h"
  "Cr2LJpegDecoder.cpp"
  "Cr2LJpegDecoder.h"
  "CrwDecompressor.cpp"
  "CrwDecompressor.h"
  "DeflateDecompressor.cpp"
  "DeflateDecompressor.h"
  "FujiDecompressor.cpp"
  "FujiDecompressor.h"
  "HasselbladDecompressor.cpp"
  "HasselbladDecompressor.h"
  "HasselbladLJpegDecoder.cpp"
  "HasselbladLJpegDecoder.h"
  "JpegDecompressor.cpp"
  "JpegDecompressor.h"
  "JpegMarkers.h"
  "KodakDecompressor.cpp"
  "KodakDecompressor.h"
  "LJpegDecoder.cpp"
  "LJpegDecoder.h"
  "LJpegDecompressor.cpp"
  "LJpegDecompressor.h"
  "NikonDecompressor.cpp"
  "NikonDecompressor.h"
  "OlympusDecompressor.cpp"
  "OlympusDecompressor.h"
  "PanasonicV4Decompressor.cpp"
  "PanasonicV4Decompressor.h"
  "PanasonicV5Decompressor.cpp"
  "PanasonicV5Decompressor.h"
  "PanasonicV6Decompressor.cpp"
  "PanasonicV6Decompressor.h"
  "PanasonicV7Decompressor.cpp"
  "PanasonicV7Decompressor.h"
  "PentaxDecompressor.cpp"
  "PentaxDecompressor.h"
  "PhaseOneDecompressor.cpp"
  "PhaseOneDecompressor.h"
  "SamsungV0Decompressor.cpp"
  "SamsungV0Decompressor.h"
  "SamsungV1Decompressor.cpp"
  "SamsungV1Decompressor.h"
  "SamsungV2Decompressor.cpp"
  "SamsungV2Decompressor.h"
  "SonyArw1Decompressor.cpp"
  "SonyArw1Decompressor.h"
  "SonyArw2Decompressor.cpp"
  "SonyArw2Decompressor.h"
  "UncompressedDecompressor.cpp"
  "UncompressedDecompressor.h"
  "VC5Decompressor.cpp"
  "VC5Decompressor.h"
)

target_sources(rawspeed_decompressors PRIVATE
  ${SOURCES}
)

target_include_directories(rawspeed_decompressors PUBLIC "${RAWSPEED_BINARY_DIR}/src")
target_include_directories(rawspeed_decompressors SYSTEM PUBLIC "${RAWSPEED_SOURCE_DIR}/src/external")
target_include_directories(rawspeed_decompressors PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")

if(TARGET RawSpeed::OpenMP_CXX)
  target_link_libraries(rawspeed_decompressors PUBLIC RawSpeed::OpenMP_CXX)
endif()

if(WITH_JPEG AND TARGET JPEG::JPEG)
  target_link_libraries(rawspeed_decompressors PUBLIC JPEG::JPEG)
endif()

target_link_libraries(rawspeed PRIVATE rawspeed_decompressors)
