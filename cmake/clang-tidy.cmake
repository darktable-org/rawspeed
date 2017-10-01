find_package(LLVMClangTidy REQUIRED)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(CMAKE_CXX_CLANG_TIDY "${CLANGTIDY_PATH}" -extra-arg=-Wno-unknown-warning-option)
else()
  set(CMAKE_CXX_CLANG_TIDY "${CLANGTIDY_PATH}")
endif()
