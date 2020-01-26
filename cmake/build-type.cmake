# Add a sensible build type default and warning because empty means no optimization and no debug info.
if(NOT CMAKE_BUILD_TYPE)
  message(WARNING "CMAKE_BUILD_TYPE is not defined!")

  set(default_build_type "RelWithDebInfo")

  if(BUILD_RS_TESTING)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      set(default_build_type "Coverage")
    else()
      set(default_build_type "Sanitize")
    endif()
  endif()

  message("WARNING: Defaulting to CMAKE_BUILD_TYPE=${default_build_type}. Use ccmake to set a proper value.")

  SET(CMAKE_BUILD_TYPE ${default_build_type} CACHE STRING "" FORCE)
endif(NOT CMAKE_BUILD_TYPE)

set(RAWSPEED_STANDARD_BUILD_TYPES Debug RelWithDebInfo Release)
set(RAWSPEED_SPECIAL_BUILD_TYPES Coverage Sanitize TSan Fuzz)
set(CMAKE_CONFIGURATION_TYPES ${RAWSPEED_STANDARD_BUILD_TYPES} ${RAWSPEED_SPECIAL_BUILD_TYPES} CACHE STRING "All the available build types" FORCE)

string(TOUPPER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_UPPERCASE)
string(TOUPPER "${CMAKE_CONFIGURATION_TYPES}" CMAKE_CONFIGURATION_TYPES_UPPERCASE)
string(TOUPPER "${RAWSPEED_SPECIAL_BUILD_TYPES}" RAWSPEED_SPECIAL_BUILD_TYPES_UPPERCASE)

# is this one of the known build types?
list (FIND CMAKE_CONFIGURATION_TYPES_UPPERCASE ${CMAKE_BUILD_TYPE_UPPERCASE} BUILD_TYPE_IS_KNOWN)
if (${BUILD_TYPE_IS_KNOWN} EQUAL -1)
 message(SEND_ERROR "Unknown build type: ${CMAKE_BUILD_TYPE_UPPERCASE}. Please specify one of: ${CMAKE_CONFIGURATION_TYPES}")
endif()

# is this a special build?
list (FIND RAWSPEED_SPECIAL_BUILD_TYPES_UPPERCASE ${CMAKE_BUILD_TYPE_UPPERCASE} IS_SPECIAL_BUILD)
if (${IS_SPECIAL_BUILD} EQUAL -1)
 unset(RAWSPEED_SPECIAL_BUILD)
else()
 set(RAWSPEED_SPECIAL_BUILD 1)
endif()

foreach(CONFIGURATION_TYPE ${CMAKE_CONFIGURATION_TYPES})
  unset(RAWSPEED_${CONFIGURATION_TYPE}_BUILD)
endforeach()
set(RAWSPEED_${CMAKE_BUILD_TYPE_UPPERCASE}_BUILD 1)
