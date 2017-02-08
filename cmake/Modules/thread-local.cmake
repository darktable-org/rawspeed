include(CheckCXXSourceCompiles)

CHECK_CXX_SOURCE_COMPILES("
static thread_local int tls;
int main(void)
{
  return 0;
}" HAVE_THREAD_LOCAL)
if(HAVE_THREAD_LOCAL)
  add_definitions(-DHAVE_THREAD_LOCAL)
  return()
endif()

CHECK_CXX_SOURCE_COMPILES("
static __thread int tls;
int main(void)
{
  return 0;
}" HAVE___THREAD)
if(HAVE___THREAD)
  add_definitions(-DHAVE___THREAD)
  return()
endif()

MESSAGE(SEND_ERROR "The compiler does not support Thread-local storage.")
