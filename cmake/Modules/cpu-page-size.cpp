#include <iostream> // for endl, basic_ostream<>::__ostream_type, cout, ost...

#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h> // for _POSIX_C_SOURCE, sysconf, _SC_PAGESIZE
#endif

#if (defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 1) || defined(__APPLE__)

int main() {
  long val = ::sysconf(_SC_PAGESIZE);
  if (val == -1)
    return 1;
  std::cout << val << std::endl;
  return 0;
}

#elif defined(_WIN32) || defined(_WIN64)

#include <Windows.h>
int main() {
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  std::cout << si.dwPageSize << std::endl;
  return 0;
}

#else
#error Do not know how to query (minimal) CPU page size for this system!
#endif
