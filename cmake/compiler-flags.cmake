include(CpuMarch)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

# always debug info
add_definitions(-g3)
add_definitions(-ggdb3)

# warnings
add_definitions(-Wall)

if(UNIX OR APPLE)
  # want -Werror to be enabled automatically for me.
  # but on windows platform, there are warnings still
  add_definitions(-Werror)
endif()

# should be fixed IMHO
add_definitions(-fno-strict-aliasing)

# assertions
if(CMAKE_BUILD_TYPE MATCHES "^[Re][Ee][Ll][Ee][Aa][Ss][Ee]$")
  add_definitions(-DNDEBUG)
else()
  add_definitions(-DDEBUG)
endif()

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")

set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -O2")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -O2")

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
