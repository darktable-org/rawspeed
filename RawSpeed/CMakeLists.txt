if(WITH_PTHREADS)
  set(CMAKE_THREAD_PREFER_PTHREAD 1)
  find_package(Threads REQUIRED)
  if(NOT CMAKE_USE_PTHREADS_INIT)
    message(SEND_ERROR "Did not found POSIX Threads! Either make it find pthreads, or pass -DWITH_PTHREADS=OFF to disable threading.")
  endif()
else()
  message(STATUS "PThread-based threading is disabled. Not searching for PThreads")
  add_definitions(-DNO_PTHREAD)
endif()

find_package(Pugixml 1.2 REQUIRED)
include_directories(SYSTEM ${Pugixml_INCLUDE_DIRS})

find_package(JPEG REQUIRED)
include_directories(SYSTEM ${JPEG_INCLUDE_DIRS})

if(BUILD_TESTING)
  # want GTEST_ADD_TESTS() macro. NOT THE ACTUAL MODULE!
  include(GTEST_ADD_TESTS)

  # for the actual gtest:

  # at least in debian, they are the package only installs their source code,
  # so if one wants to use them, he needs to compile them in-tree
  include(GoogleTest)
endif()

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

if(WITH_PTHREADS AND Threads_FOUND)
  target_link_libraries(rawspeed ${CMAKE_THREAD_LIBS_INIT})
endif()

if(BUILD_TESTING)
  target_link_libraries(rawspeed libgoogletest)
endif()

if(BUILD_TESTING)
  FILE(GLOB RAWSPEED_TEST_SOURCES
    "RawSpeedTest.cpp"
  )

  foreach(IN ${RAWSPEED_TEST_SOURCES})
    get_filename_component(TESTNAME ${IN} NAME_WE)

    add_executable(${TESTNAME} ${IN})
    target_link_libraries(${TESTNAME} rawspeed)
    GTEST_ADD_TESTS(${TESTNAME} "" AUTO)
  endforeach()
endif()


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
