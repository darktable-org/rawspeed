include(CheckCXXCompilerFlagAndEnableIt)

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  return()
endif()

set (CLANG_WARNING_FLAGS
  "all"
  "extra"
  "everything"
)

set (CLANG_DISABLED_WARNING_FLAGS
  "c++98-compat"
  "c++98-compat-pedantic"
  "conversion" # FIXME: really need to enable this one
  "covered-switch-default"
  "deprecated"
  "double-promotion"
  "exit-time-destructors"
  "global-constructors"
  "gnu-zero-variadic-macro-arguments"
  "old-style-cast"
  "padded"
  "switch-enum"
  "unused-macros"
  "unused-parameter"
  "weak-vtables"
  "zero-as-null-pointer-constant" # temporary
)

# Yes, these have to be *re-enabled* after CLANG_DISABLED_WARNING_FLAGS.
set (CLANG_REENABLED_WARNING_FLAGS
  "extra-semi"
)

set(CMAKE_REQUIRED_FLAGS_ORIG "${CMAKE_REQUIRED_FLAGS}")
set(CMAKE_REQUIRED_FLAGS "-c -Wunreachable-code -Werror=unreachable-code")
# see https://reviews.llvm.org/D25321
# see https://github.com/darktable-org/rawspeed/issues/104
CHECK_CXX_SOURCE_COMPILES(
  "void foo() {
  return;
  __builtin_unreachable();
}"
  CLANG_CXX_FLAG_UNREACHABLE_CODE_WORKS
)
set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS_ORIG}")

if(NOT CLANG_CXX_FLAG_UNREACHABLE_CODE_WORKS)
  list(APPEND CLANG_DISABLED_WARNING_FLAGS "unreachable-code")
endif()

set(CMAKE_REQUIRED_FLAGS_ORIG "${CMAKE_REQUIRED_FLAGS}")
set(CMAKE_REQUIRED_FLAGS "-c -Wmissing-braces -Werror=missing-braces")
# see https://bugs.llvm.org/show_bug.cgi?id=21629
CHECK_CXX_SOURCE_COMPILES(
"#include <array>
const std::array<int, 2> test = {0, 0};"
  CLANG_CXX_FLAG_MISSING_BRACES_WORKS
)
set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS_ORIG}")

if(NOT CLANG_CXX_FLAG_MISSING_BRACES_WORKS)
  list(APPEND CLANG_DISABLED_WARNING_FLAGS "missing-braces")
endif()

set(CMAKE_REQUIRED_FLAGS_ORIG "${CMAKE_REQUIRED_FLAGS}")
set(CMAKE_REQUIRED_FLAGS "-c -Wthread-safety-analysis -Werror=thread-safety-analysis")
CHECK_CXX_SOURCE_COMPILES(
"// Enable thread safety attributes only with clang.
// The attributes can be safely erased when compiling with other compilers.
#if defined(__clang__) && (!defined(SWIG))
#define THREAD_ANNOTATION_ATTRIBUTE__(x) __attribute__((x))
#else
#define THREAD_ANNOTATION_ATTRIBUTE__(x) // no-op
#endif

#define CAPABILITY(x) THREAD_ANNOTATION_ATTRIBUTE__(capability(x))
#define REQUIRES(...) THREAD_ANNOTATION_ATTRIBUTE__(requires_capability(__VA_ARGS__))

class CAPABILITY(\"mutex\") Mutex {
public:
  const Mutex& operator!() const { return *this; }
};

class ErrorLog {
public:
  Mutex mutex;

  void test() REQUIRES(!mutex);
};

void test(ErrorLog e) {
    e.test();
}
"
  CLANG_CXX_THREAD_SAFETY_ANALYSIS_NEGATIVE_CAPABILITIES_WORK
)
set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS_ORIG}")

if(NOT CLANG_CXX_THREAD_SAFETY_ANALYSIS_NEGATIVE_CAPABILITIES_WORK)
  list(APPEND CLANG_DISABLED_WARNING_FLAGS "thread-safety-analysis")
endif()

if(NOT (UNIX OR APPLE))
  # bogus warnings about std functions...
  list(APPEND CLANG_DISABLED_WARNING_FLAGS "used-but-marked-unused")
  # just don't care.
  list(APPEND CLANG_DISABLED_WARNING_FLAGS "nonportable-system-include-path")
endif()

foreach(warning ${CLANG_WARNING_FLAGS})
  CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-W${warning})
endforeach()

foreach(warning ${CLANG_DISABLED_WARNING_FLAGS})
  CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wno-${warning})
endforeach()

foreach(warning ${CLANG_REENABLED_WARNING_FLAGS})
  CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-W${warning})
endforeach()
