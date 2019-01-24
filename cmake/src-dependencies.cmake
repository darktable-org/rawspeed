include(FeatureSummary)

if(BUILD_TESTING)
  # want GTEST_ADD_TESTS() macro. NOT THE ACTUAL MODULE!
  include(GTEST_ADD_TESTS)

  # for the actual gtest:

  # at least in debian, they are the package only installs their source code,
  # so if one wants to use them, he needs to compile them in-tree
  include(GoogleTest)

  add_dependencies(dependencies gtest gmock_main)
endif()

if(BUILD_BENCHMARKING)
  include(GoogleBenchmark)

  add_dependencies(dependencies benchmark)
endif()

unset(HAVE_OPENMP)
if(WITH_OPENMP)
  message(STATUS "Looking for OpenMP")

  if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR
     CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    # Clang has an option to specify the OpenMP standard to use. Specify it.
    set(OPENMP_VERSION_SPECIFIER "-fopenmp-version=40")
  endif()

  set(CMAKE_C_FLAGS_SAVE "${CMAKE_C_FLAGS}")
  set(CMAKE_CXX_FLAGS_SAVE "${CMAKE_CXX_FLAGS}")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OPENMP_VERSION_SPECIFIER}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OPENMP_VERSION_SPECIFIER}")

  find_package(OpenMP 4.0)

  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS_SAVE}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_SAVE}")

  if(NOT OPENMP_FOUND)
    message(SEND_ERROR "Did not find OpenMP! Either make it find OpenMP, "
                       "or pass -DWITH_OPENMP=OFF to disable OpenMP support.")
  else()
    message(STATUS "Looking for OpenMP - found (system)")
  endif()

  # FIXME: OpenMP::OpenMP_CXX target, and ${OpenMP_CXX_LIBRARIES} were both
  # added in cmake-3.9. Until then, this is correct:
  if(NOT TARGET OpenMP::OpenMP_CXX)
    add_library(OpenMP::OpenMP_CXX INTERFACE IMPORTED)
    if(OpenMP_CXX_FLAGS)
      set_property(TARGET OpenMP::OpenMP_CXX PROPERTY INTERFACE_COMPILE_OPTIONS ${OpenMP_CXX_FLAGS})
      set_property(TARGET OpenMP::OpenMP_CXX PROPERTY INTERFACE_LINK_LIBRARIES ${OpenMP_CXX_FLAGS})
      # Yes, both of them to the same value.
    endif()
  endif()

  if(NOT USE_BUNDLED_LLVMOPENMP)
    target_link_libraries(rawspeed PUBLIC OpenMP::OpenMP_CXX)
    target_compile_options(rawspeed PUBLIC ${OPENMP_VERSION_SPECIFIER})
  else()
    include(LLVMOpenMP)

    message(STATUS "Looking for OpenMP - found 'in-tree' runtime library")

    add_dependencies(dependencies omp)
    target_compile_options(rawspeed PUBLIC $<TARGET_PROPERTY:OpenMP::OpenMP_CXX,INTERFACE_COMPILE_OPTIONS>) # compile time only!
    target_compile_options(rawspeed PUBLIC ${OPENMP_VERSION_SPECIFIER})
    target_link_libraries(rawspeed PUBLIC omp) # newly built runtime library
  endif()

  set(HAVE_OPENMP 1)

  set_package_properties(OpenMP PROPERTIES
                         TYPE RECOMMENDED
                         URL https://www.openmp.org/
                         DESCRIPTION "Open Multi-Processing"
                         PURPOSE "Used for parallelization of the library")
else()
  message(STATUS "OpenMP is disabled")
endif()
add_feature_info("OpenMP-based threading" HAVE_OPENMP "used for parallelization of the library")

unset(HAVE_PUGIXML)
if(WITH_PUGIXML)
  message(STATUS "Looking for pugixml")
  if(NOT USE_BUNDLED_PUGIXML)
    find_package(Pugixml 1.2)
    if(NOT Pugixml_FOUND)
      message(SEND_ERROR "Did not find Pugixml! Either make it find Pugixml, or pass -DUSE_BUNDLED_PUGIXML=ON to enable in-tree pugixml.")
    else()
      message(STATUS "Looking for pugixml - found (system)")
    endif()
  else()
    include(Pugixml)
    if(NOT Pugixml_FOUND)
      message(SEND_ERROR "Managed to fail to use 'bundled' Pugixml!")
    else()
      message(STATUS "Looking for pugixml - found ('in-tree')")
      add_dependencies(dependencies ${Pugixml_LIBRARIES})
    endif()
  endif()

  if(Pugixml_FOUND)
    set(HAVE_PUGIXML 1)

    if(NOT TARGET Pugixml::Pugixml)
      add_library(Pugixml::Pugixml INTERFACE IMPORTED)
      set_property(TARGET Pugixml::Pugixml PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${Pugixml_INCLUDE_DIRS}")
      set_property(TARGET Pugixml::Pugixml PROPERTY INTERFACE_LINK_LIBRARIES "${Pugixml_LIBRARIES}")
    endif()

    target_link_libraries(rawspeed PRIVATE Pugixml::Pugixml)
    set_package_properties(Pugixml PROPERTIES
                           TYPE REQUIRED
                           URL http://pugixml.org/
                           DESCRIPTION "Light-weight, simple and fast XML parser"
                           PURPOSE "Used for loading of data/cameras.xml")
  endif()
else()
  message(STATUS "Pugixml library support is disabled. I hope you know what you are doing.")
endif()
add_feature_info("XML reading" HAVE_PUGIXML "used for loading of data/cameras.xml")

unset(HAVE_JPEG)
if(WITH_JPEG)
  message(STATUS "Looking for JPEG")
  find_package(JPEG)
  if(NOT JPEG_FOUND)
    message(SEND_ERROR "Did not find JPEG! Either make it find JPEG, or pass -DWITH_JPEG=OFF to disable JPEG.")
  else()
    message(STATUS "Looking for JPEG - found")
    set(HAVE_JPEG 1)

    if(NOT TARGET JPEG::JPEG)
      add_library(JPEG::JPEG INTERFACE IMPORTED)
      set_property(TARGET JPEG::JPEG PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${JPEG_INCLUDE_DIRS}")
      set_property(TARGET JPEG::JPEG PROPERTY INTERFACE_LINK_LIBRARIES "${JPEG_LIBRARIES}")
    endif()

    target_link_libraries(rawspeed PUBLIC JPEG::JPEG)
    set_package_properties(JPEG PROPERTIES
                           TYPE RECOMMENDED
                           DESCRIPTION "free library for handling the JPEG image data format, implements a JPEG codec"
                           PURPOSE "Used for decoding DNG Lossy JPEG compression")

    include(CheckJPEGSymbols)
  endif()
else()
  message(STATUS "JPEG is disabled, DNG Lossy JPEG support won't be available.")
endif()
add_feature_info("Lossy JPEG decoding" HAVE_JPEG "used for DNG Lossy JPEG compression decoding")

unset(HAVE_ZLIB)
if (WITH_ZLIB)
  message(STATUS "Looking for ZLIB")
  if(NOT USE_BUNDLED_ZLIB)
    find_package(ZLIB)
    if(NOT ZLIB_FOUND)
      message(SEND_ERROR "Did not find ZLIB! Either make it find ZLIB, or pass -DWITH_ZLIB=OFF to disable ZLIB, or pass -DUSE_BUNDLED_ZLIB=ON to enable in-tree ZLIB.")
    else()
      include(CheckZLIB)
      message(STATUS "Looking for ZLIB - found (system)")
    endif()
  else()
    include(Zlib)
    if(NOT ZLIB_FOUND)
      message(SEND_ERROR "Managed to fail to use 'bundled' ZLIB!")
    else()
      message(STATUS "Looking for ZLIB - found ('in-tree')")
      add_dependencies(dependencies ${ZLIB_LIBRARIES})
    endif()
  endif()

  if(ZLIB_FOUND)
    set(HAVE_ZLIB 1)

    if(NOT TARGET ZLIB::ZLIB)
      add_library(ZLIB::ZLIB INTERFACE IMPORTED)
      set_property(TARGET ZLIB::ZLIB PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${ZLIB_INCLUDE_DIRS}")
      set_property(TARGET ZLIB::ZLIB PROPERTY INTERFACE_LINK_LIBRARIES "${ZLIB_LIBRARIES}")
    endif()

    target_link_libraries(rawspeed PUBLIC ZLIB::ZLIB)
    set_package_properties(ZLIB PROPERTIES
                           TYPE RECOMMENDED
                           DESCRIPTION "software library used for data compression"
                           PURPOSE "Used for decoding DNG Deflate compression")
    endif()
else()
  message(STATUS "ZLIB is disabled, DNG deflate support won't be available.")
endif()
add_feature_info("ZLIB decoding" HAVE_ZLIB "used for DNG Deflate compression decoding")
