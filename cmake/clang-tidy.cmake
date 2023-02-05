find_package(LLVMClangTidy REQUIRED)

set(CMAKE_CXX_CLANG_TIDY "${CLANGTIDY_PATH}" -extra-arg=-Wno-unknown-warning-option)
