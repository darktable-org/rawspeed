cmake_minimum_required(VERSION 3.0)

# Enable ExternalProject CMake module
include(ExternalProject)

# Download and install GoogleTest
ExternalProject_Add(
    googletest
    URL /usr/src/googletest/
    PREFIX ${CMAKE_BINARY_DIR}/googletest
    # Disable install step
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS ${CMAKE_BINARY_DIR}/googletest/src/googletest-build/googlemock/libgmock.a
)

# Create a libgoogletest target to be used as a dependency by test programs
add_library(libgoogletest IMPORTED STATIC GLOBAL)
add_dependencies(libgoogletest googletest)

# Set googletest properties
ExternalProject_Get_Property(googletest source_dir binary_dir)

# avoid INTERFACE_INCLUDE_DIRECTORIES not found issue
file(MAKE_DIRECTORY "${source_dir}/include")

set_target_properties(libgoogletest PROPERTIES
    "IMPORTED_LOCATION" "${binary_dir}/googlemock/libgmock.a"
    "IMPORTED_LINK_INTERFACE_LIBRARIES" "${CMAKE_THREAD_LIBS_INIT}"
    "INTERFACE_INCLUDE_DIRECTORIES" "${source_dir}/include"
    )
