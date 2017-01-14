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

if(WITH_OPENMP)
  find_package(OpenMP)
endif()

find_package(Pugixml 1.2 REQUIRED)
include_directories(SYSTEM ${Pugixml_INCLUDE_DIRS})

find_package(JPEG REQUIRED)
include_directories(SYSTEM ${JPEG_INCLUDE_DIRS})

if (WITH_ZLIB)
  find_package(ZLIB REQUIRED)
  if(NOT ZLIB_FOUND)
    message(SEND_ERROR "Did not find ZLIB! Either make it find ZLIB, or pass -DWITH_ZLIB=OFF to disable ZLIB.")
  endif()
  add_definitions(-DHAVE_ZLIB)
  include_directories(SYSTEM ${ZLIB_INCLUDE_DIRS})
else()
  message(STATUS "ZLIB is disabled, DNG deflate support won't be available.")
endif()

CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

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

if(WITH_ZLIB AND ZLIB_FOUND)
  list(APPEND RAWSPEED_LIBS ${ZLIB_LIBRARIES})
endif()

if(WITH_PTHREADS AND Threads_FOUND)
  list(APPEND RAWSPEED_LIBS ${CMAKE_THREAD_LIBS_INIT})
endif()

add_library(rawspeed_static STATIC $<TARGET_OBJECTS:rawspeed>)
target_link_libraries(rawspeed_static PUBLIC ${RAWSPEED_LIBS})
add_dependencies(rawspeed_static rawspeed)

if(WITH_OPENMP AND OPENMP_FOUND)
  target_link_libraries(rawspeed_static INTERFACE ${OpenMP_CXX_FLAGS})
endif()

if(BUILD_TESTING)
  add_custom_target(tests ALL)

  FILE(GLOB RAWSPEED_TEST_SOURCES
    "RawSpeed.cpp"
  )

  if(CMAKE_BUILD_TYPE MATCHES "^[Cc][Oo][Vv][Ee][Rr][Aa][Gg][Ee]$")
    # want all the symbols.
    add_library(rawspeed_test SHARED $<TARGET_OBJECTS:rawspeed> ${RAWSPEED_TEST_SOURCES})
  else()
    add_library(rawspeed_test STATIC $<TARGET_OBJECTS:rawspeed> ${RAWSPEED_TEST_SOURCES})
  endif()
  target_link_libraries(rawspeed_test PUBLIC ${RAWSPEED_LIBS})
  add_dependencies(rawspeed_test rawspeed)

  target_link_libraries(rawspeed_test PUBLIC gtest gmock_main)
  target_include_directories(rawspeed_test INTERFACE ${CMAKE_CURRENT_BINARY_DIR})

  if(WITH_OPENMP AND OPENMP_FOUND)
    target_link_libraries(rawspeed_test INTERFACE ${OpenMP_CXX_FLAGS})
  endif()

  FILE(GLOB RAWSPEED_TESTS_SOURCES
    "BlackAreaTest.cpp"
    "CameraMetaDataTest.cpp"
    "CameraSensorInfoTest.cpp"
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

if(BUILD_TOOLS)
  add_executable(darktable-rs-identify rawspeed-identify.cpp)
  target_compile_definitions(darktable-rs-identify
    PRIVATE -DRS_CAMERAS_XML_PATH="${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATAROOTDIR}/darktable/rawspeed/cameras.xml"
  )

  set_target_properties(darktable-rs-identify PROPERTIES LINKER_LANGUAGE CXX)

  target_link_libraries(darktable-rs-identify rawspeed_static)

  install(TARGETS darktable-rs-identify DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

if(BUILD_TESTING AND BUILD_TOOLS)
  add_executable(rstest rstest.cpp md5.cpp)
  set_target_properties(rstest PROPERTIES LINKER_LANGUAGE CXX)
  target_link_libraries(rstest rawspeed_test)
endif()
