FILE(GLOB DECOMPRESSOR_SOURCES
  "AbstractLJpegDecompressor.cpp"
  "AbstractLJpegDecompressor.h"
  "Cr2Decompressor.cpp" # IWYU breaks
  "Cr2Decompressor.h"
  "DeflateDecompressor.cpp"
  "DeflateDecompressor.h"
  "HasselbladDecompressor.cpp" # IWYU breaks
  "HasselbladDecompressor.h"
  "HuffmanTable.h"
  "JpegDecompressor.cpp"
  "JpegDecompressor.h"
  "LJpegDecompressor.cpp" # IWYU breaks
  "LJpegDecompressor.h"
  "NikonDecompressor.cpp"
  "NikonDecompressor.h"
  "PentaxDecompressor.cpp"
  "PentaxDecompressor.h"
  "UncompressedDecompressor.cpp"
  "UncompressedDecompressor.h"
)

set(RAWSPEED_SOURCES "${RAWSPEED_SOURCES};${DECOMPRESSOR_SOURCES}" PARENT_SCOPE)
