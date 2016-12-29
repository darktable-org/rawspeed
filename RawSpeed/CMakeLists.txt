if(BUILD_TESTING)
  # want GTEST_ADD_TESTS() macro. NOT THE ACTUAL MODULE!
  include(GTEST_ADD_TESTS)

  # for the actual gtest:

  # at least in debian, they are the package only installs their source code,
  # so if one wants to use them, he needs to compile them in-tree
  include(GoogleTest)
endif()

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

add_library(rawspeed OBJECT ${RAWSPEED_SOURCES})

list(APPEND RAWSPEED_LIBS ${Pugixml_LIBRARIES} ${JPEG_LIBRARIES})

if(WITH_PTHREADS AND Threads_FOUND)
  list(APPEND RAWSPEED_LIBS ${CMAKE_THREAD_LIBS_INIT})
endif()

add_library(rawspeed_static STATIC $<TARGET_OBJECTS:rawspeed>)
target_link_libraries(rawspeed_static ${RAWSPEED_LIBS})
add_dependencies(rawspeed_static rawspeed)

if(BUILD_TESTING)
  add_custom_target(tests ALL)

  FILE(GLOB RAWSPEED_TEST_SOURCES
    "RawSpeed.cpp"
  )

  if((APPLE AND NOT (CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")) OR (NOT CMAKE_BUILD_TYPE MATCHES "^[Cc][Oo][Vv][Ee][Rr][Aa][Gg][Ee]$"))
    # standard library mismatch issues, will probably end up dropping GCC on OSX completely
    add_library(rawspeed_test STATIC $<TARGET_OBJECTS:rawspeed> ${RAWSPEED_TEST_SOURCES})
  else()
    add_library(rawspeed_test SHARED $<TARGET_OBJECTS:rawspeed> ${RAWSPEED_TEST_SOURCES})
  endif()
  target_link_libraries(rawspeed_test ${RAWSPEED_LIBS})
  add_dependencies(rawspeed_test rawspeed)

  target_link_libraries(rawspeed_test gtest gmock_main)

  FILE(GLOB RAWSPEED_TESTS_SOURCES
    "BlackAreaTest.cpp"
    "ExceptionsTest.cpp"
    "RawSpeedTest.cpp"
  )

  foreach(IN ${RAWSPEED_TESTS_SOURCES})
    get_filename_component(TESTNAME ${IN} NAME_WE)

    add_executable(${TESTNAME} ${IN})

    target_link_libraries(${TESTNAME} rawspeed_test)

    GTEST_ADD_TESTS(${TESTNAME} "" AUTO)

    add_dependencies(tests ${TESTNAME})
  endforeach()
endif()
