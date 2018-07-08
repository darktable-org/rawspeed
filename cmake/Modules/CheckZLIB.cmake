include(CheckIncludeFileCXX)
include(CheckTypeSize)
include(CheckPrototypeDefinition)
include(CheckCXXSymbolExists)

enable_language(C)

set(CMAKE_REQUIRED_INCLUDES_SAVE "${CMAKE_REQUIRED_INCLUDES}")
set(CMAKE_EXTRA_INCLUDE_FILES_SAVE "${CMAKE_EXTRA_INCLUDE_FILES}")
set(CMAKE_REQUIRED_LIBRARIES_SAVE "${CMAKE_REQUIRED_LIBRARIES}")

set(CMAKE_REQUIRED_INCLUDES "${CMAKE_REQUIRED_INCLUDES_SAVE};${ZLIB_INCLUDE_DIRS}")

CHECK_INCLUDE_FILE_CXX("zlib.h" HAVE_ZLIB_H)
if(NOT HAVE_ZLIB_H)
  message(SEND_ERROR "Did not find <zlib.h> header")
endif()

set(CMAKE_EXTRA_INCLUDE_FILES "zlib.h")

CHECK_TYPE_SIZE(uLongf HAVE_ZLIB_ULONGF)
if(NOT HAVE_ZLIB_ULONGF)
  message(SEND_ERROR "Did not find uLongf type in <zlib.h>")
endif()

CHECK_CXX_SYMBOL_EXISTS(Z_OK "zlib.h" HAVE_ZLIB_Z_OK)
if(NOT HAVE_ZLIB_Z_OK)
  message(SEND_ERROR "Did not find Z_OK macro in <zlib.h>")
endif()

CHECK_PROTOTYPE_DEFINITION(uncompress
 "int uncompress(unsigned char* dest, uLongf* destLen, const unsigned char* source, unsigned long sourceLen)"
 "Z_OK"
 "zlib.h"
 HAVE_ZLIB_UNCOMPRESS_PROTOTYPE)
if(NOT HAVE_ZLIB_UNCOMPRESS_PROTOTYPE)
  message(SEND_ERROR "Found unexpected prototype for uncompress() in <zlib.h>")
endif()

CHECK_PROTOTYPE_DEFINITION(zError
 "const char* zError(int zErrorCode)"
 "NULL"
 "zlib.h"
 HAVE_ZLIB_ZERROR_PROTOTYPE)
if(NOT HAVE_ZLIB_ZERROR_PROTOTYPE)
  message(SEND_ERROR "Found unexpected prototype for zError() in <zlib.h>")
endif()

set(CMAKE_REQUIRED_LIBRARIES "${CMAKE_REQUIRED_LIBRARIES_SAVE};${ZLIB_LIBRARIES}")

CHECK_CXX_SYMBOL_EXISTS(uncompress "zlib.h" HAVE_ZLIB_UNCOMPRESS)
if(NOT HAVE_ZLIB_UNCOMPRESS)
  message(SEND_ERROR "Did not find uncompress() function in ZLIB")
endif()

CHECK_CXX_SYMBOL_EXISTS(zError "zlib.h" HAVE_ZLIB_ZERROR)
if(NOT HAVE_ZLIB_ZERROR)
  message(SEND_ERROR "Did not find zError() function in ZLIB")
endif()

set(CMAKE_REQUIRED_INCLUDES "${CMAKE_REQUIRED_INCLUDES_SAVE}")
set(CMAKE_REQUIRED_LIBRARIES "${CMAKE_REQUIRED_LIBRARIES_SAVE}")
set(CMAKE_EXTRA_INCLUDE_FILES "${CMAKE_EXTRA_INCLUDE_FILES_SAVE}")
