# Add a sensible build type default and warning because empty means no optimization and no debug info.
if(NOT CMAKE_BUILD_TYPE)
  message("WARNING: CMAKE_BUILD_TYPE is not defined!")

  set(default_build_type "RelWithDebInfo")

  if(BUILD_TESTING)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      set(default_build_type "Coverage")
    else()
      set(default_build_type "SANITIZE")
    endif()
  endif()

  message("WARNING: Defaulting to CMAKE_BUILD_TYPE=${default_build_type}. Use ccmake to set a proper value.")

  SET(CMAKE_BUILD_TYPE ${default_build_type} CACHE STRING "" FORCE)
endif(NOT CMAKE_BUILD_TYPE)

# yes, these build types need to be specified here in upper-case.
set(SPECIAL_BUILD_TYPES COVERAGE SANITIZE TSAN FUZZ)
set(CMAKE_CONFIGURATION_TYPES Debug RelWithDebInfo Release ${SPECIAL_BUILD_TYPES})
set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES}" CACHE STRING "All the available build types" FORCE)

string(TOUPPER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_UPPERCASE)
SET(CMAKE_BUILD_TYPE_UPPERCASE "${CMAKE_BUILD_TYPE_UPPERCASE}" CACHE STRING "Choose the type of build, options are: ${CMAKE_CONFIGURATION_TYPES}." FORCE )

# is this one of the known build types?
list (FIND CMAKE_CONFIGURATION_TYPES ${CMAKE_BUILD_TYPE_UPPERCASE} BUILD_TYPE)
if (${BUILD_TYPE} EQUAL -1)
 message(SEND_ERROR "Unknown build type: ${CMAKE_BUILD_TYPE}. Please specify one of: ${CMAKE_CONFIGURATION_TYPES}")
endif()

# is this a special build?
list (FIND SPECIAL_BUILD_TYPES ${CMAKE_BUILD_TYPE} SPECIAL_BUILD)
if (${SPECIAL_BUILD} EQUAL -1)
 unset(SPECIAL_BUILD)
else()
 set(SPECIAL_BUILD 1)
endif()
