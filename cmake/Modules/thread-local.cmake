include(CheckCXXSourceCompiles)

CHECK_CXX_SOURCE_COMPILES("
static thread_local int tls;
int main(void)
{
  (void)tls;
  return 0;
}" HAVE_CXX_THREAD_LOCAL)
if(HAVE_CXX_THREAD_LOCAL)
  return()
endif()

CHECK_CXX_SOURCE_COMPILES("
static __thread int tls;
int main(void)
{
  (void)tls;
  return 0;
}" HAVE_GCC_THREAD_LOCAL)
if(HAVE_GCC_THREAD_LOCAL)
  return()
endif()

MESSAGE(SEND_ERROR "The compiler does not support Thread-local storage.")
