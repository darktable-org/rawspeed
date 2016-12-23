find_package(Pugixml 1.2 REQUIRED)
include_directories(SYSTEM ${Pugixml_INCLUDE_DIRS})

find_package(JPEG REQUIRED)
include_directories(SYSTEM ${JPEG_INCLUDE_DIRS})

FILE(GLOB RAWSPEED_SOURCES
  "AriDecoder.cpp"
  "ArwDecoder.cpp"
  "BitPumpJPEG.cpp"
  "BitPumpMSB.cpp"
  "BitPumpMSB16.cpp"
  "BitPumpMSB32.cpp"
  "BitPumpPlain.cpp"
  "BlackArea.cpp"
  "ByteStream.cpp"
  "ByteStreamSwap.cpp"
  "Camera.cpp"
  "CameraMetaData.cpp"
  "CameraMetadataException.cpp"
  "CameraSensorInfo.cpp"
  "CiffEntry.cpp"
  "CiffIFD.cpp"
  "CiffParser.cpp"
  "CiffParserException.cpp"
  "ColorFilterArray.cpp"
  "Common.cpp"
  "Cr2Decoder.cpp"
  "CrwDecoder.cpp"
  "DcrDecoder.cpp"
  "DcsDecoder.cpp"
  "DngDecoder.cpp"
  "DngDecoderSlices.cpp"
  "DngOpcodes.cpp"
  "ErfDecoder.cpp"
  "FileIOException.cpp"
  "FileMap.cpp"
  "FileReader.cpp"
  "FileWriter.cpp"
  "HasselbladDecompressor.cpp"
  "IOException.cpp"
  "KdcDecoder.cpp"
  "LJpegDecompressor.cpp"
  "LJpegPlain.cpp"
  "MefDecoder.cpp"
  "MosDecoder.cpp"
  "MrwDecoder.cpp"
  "NakedDecoder.cpp"
  "NefDecoder.cpp"
  "NikonDecompressor.cpp"
  "OrfDecoder.cpp"
  "PefDecoder.cpp"
  "PentaxDecompressor.cpp"
  "RafDecoder.cpp"
  "RawDecoder.cpp"
  "RawDecoderException.cpp"
  "RawImage.cpp"
  "RawImageDataFloat.cpp"
  "RawImageDataU16.cpp"
  "RawParser.cpp"
  "Rw2Decoder.cpp"
  "SrwDecoder.cpp"
  "StdAfx.cpp"
  "ThreefrDecoder.cpp"
  "TiffEntry.cpp"
  "TiffEntryBE.cpp"
  "TiffIFD.cpp"
  "TiffIFDBE.cpp"
  "TiffParser.cpp"
  "TiffParserException.cpp"
  "X3fDecoder.cpp"
  "X3fParser.cpp"
)

#
# build librawspeed
#
if(WIN32)
  set(RAWSPEED_LIBS "msvcrt")
endif(WIN32)

add_library(rawspeed STATIC ${RAWSPEED_SOURCES})

set_target_properties(rawspeed
  PROPERTIES
    POSITION_INDEPENDENT_CODE True
)

target_link_libraries(rawspeed ${RAWSPEED_LIBS} ${Pugixml_LIBRARIES} ${JPEG_LIBRARIES})

#
# the rawspeed part is a bit of a hack:
# the static linking didn't work since it was pulling -lstdc++ and -lm into linker flags.
# so we do a custom dependency and pretend an imported librawsped.a so no other -l are
# appended.
#
add_library(rawspeed_static STATIC IMPORTED GLOBAL)
set_target_properties(rawspeed_static
  PROPERTIES
    IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/librawspeed.a
    INTERFACE_LINK_LIBRARIES $<TARGET_PROPERTY:rawspeed,INTERFACE_LINK_LIBRARIES>
    POSITION_INDEPENDENT_CODE True
)

add_dependencies(rawspeed_static rawspeed)
