FILE(GLOB DECOMPRESSOR_SOURCES
  "AbstractLJpegDecompressor.cpp"
  "AbstractLJpegDecompressor.h"
  "Cr2Decompressor.cpp"
  "Cr2Decompressor.h"
  "DeflateDecompressor.cpp"
  "DeflateDecompressor.h"
  "HasselbladDecompressor.cpp"
  "HasselbladDecompressor.h"
  "HuffmanTable.h"
  "JpegDecompressor.cpp"
  "JpegDecompressor.h"
  "LJpegDecompressor.cpp"
  "LJpegDecompressor.h"
  "NikonDecompressor.cpp"
  "NikonDecompressor.h"
  "PentaxDecompressor.cpp"
  "PentaxDecompressor.h"
  "UncompressedDecompressor.cpp"
  "UncompressedDecompressor.h"
)

set(RAWSPEED_SOURCES "${RAWSPEED_SOURCES};${DECOMPRESSOR_SOURCES}" PARENT_SCOPE)
