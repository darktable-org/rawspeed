add_subdirectory(common)
add_subdirectory(metadata)
add_subdirectory(io)
add_subdirectory(tiff)
add_subdirectory(parsers)
add_subdirectory(decompressors)
add_subdirectory(decoders)

# list(SORT RAWSPEED_SOURCES)
list(REMOVE_DUPLICATES RAWSPEED_SOURCES)

#
# build librawspeed
#
if(WIN32)
  set(RAWSPEED_LIBS "msvcrt")
endif(WIN32)

add_library(rawspeed OBJECT ${RAWSPEED_SOURCES})

target_include_directories(rawspeed PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

list(APPEND RAWSPEED_LIBS ${Pugixml_LIBRARIES})

if(WITH_JPEG AND JPEG_FOUND)
  list(APPEND RAWSPEED_LIBS ${JPEG_LIBRARIES})
endif()

if(WITH_ZLIB AND ZLIB_FOUND)
  list(APPEND RAWSPEED_LIBS ${ZLIB_LIBRARIES})
endif()

if(WITH_PTHREADS AND Threads_FOUND)
  list(APPEND RAWSPEED_LIBS ${CMAKE_THREAD_LIBS_INIT})
endif()

add_library(rawspeed_static STATIC $<TARGET_OBJECTS:rawspeed>)
target_include_directories(rawspeed_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(rawspeed_static PUBLIC ${RAWSPEED_LIBS})
add_dependencies(rawspeed_static rawspeed)

if(WITH_OPENMP AND OPENMP_FOUND)
  target_compile_options(rawspeed_static INTERFACE ${OpenMP_CXX_FLAGS})
  target_link_libraries(rawspeed_static INTERFACE ${OpenMP_CXX_FLAGS})
endif()

if(BUILD_TESTING)
  add_subdirectory(test)
endif()
