# Find libjxl
# Will define:
# - JXL_FOUND
# - JXL_INCLUDE_DIRS directory to include for libjxl headers
# - JXL_LIBRARIES libraries to link to
# - JXL_VERSION

find_package(PkgConfig QUIET REQUIRED)
pkg_check_modules(JXL_PKGCONF QUIET libjxl)

if(JXL_PKGCONF_VERSION)
  set(JXL_VERSION ${JXL_PKGCONF_VERSION})
endif()

find_path(JXL_INCLUDE_DIR
  NAMES jxl/decode.h
  HINTS ${JXL_PKGCONF_INCLUDE_DIRS})
mark_as_advanced(JXL_INCLUDE_DIR)

find_library(JXL_LIBRARY
  NAMES jxl
  HINTS ${JXL_PKGCONF_LIBRARY_DIRS})
mark_as_advanced(JXL_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(JXL
  REQUIRED_VARS JXL_LIBRARY JXL_INCLUDE_DIR
  VERSION_VAR JXL_VERSION)

if(JXL_FOUND)
  set(JXL_INCLUDE_DIRS ${JXL_INCLUDE_DIR})
  set(JXL_LIBRARIES ${JXL_LIBRARY})
endif()
