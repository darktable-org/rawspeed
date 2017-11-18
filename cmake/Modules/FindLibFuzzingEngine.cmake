# - Try to find LibFuzzingEngine
# Once done, this will define
#
#  LibFuzzingEngine_FOUND - system has LibFuzzingEngine
#  LibFuzzingEngine_LIBRARIES - link these to use LibFuzzingEngine

include(LibFindMacros)

if(EXISTS "${LIB_FUZZING_ENGINE}")
  set(LibFuzzingEngine_LIBRARY "${LIB_FUZZING_ENGINE}")
else()
  set(LibFuzzingEngine_LIBRARY "LibFuzzingEngine_LIBRARY-NOTFOUND")
endif()

# Set the include dir variables and the libraries and let libfind_process do the rest.
# NOTE: Singular variables for this library, plural for libraries this this lib depends on.
set(LibFuzzingEngine_PROCESS_LIBS LibFuzzingEngine_LIBRARY)
libfind_process(LibFuzzingEngine)

if(LibFuzzingEngine_FOUND)
  add_library(LibFuzzingEngine INTERFACE IMPORTED)
  set_property(TARGET LibFuzzingEngine PROPERTY INTERFACE_LINK_LIBRARIES "${LibFuzzingEngine_LIBRARIES}")
endif()

set_package_properties(LibFuzzingEngine PROPERTIES
                       TYPE REQUIRED
                       DESCRIPTION "A prebuilt fuzzing engine library (e.g. libFuzzer) that needs to be linked with all fuzz target"
                       PURPOSE "Used to actually drive the fuzz targets")
