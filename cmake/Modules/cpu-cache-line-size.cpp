#include <iostream> // for endl, basic_ostream<>::__ostream_type, cout, ost...

#if defined(__unix__)
#include <unistd.h> // for _POSIX_C_VERSION, sysconf, _SC_LEVEL1_DCACHE_LINESIZE
#endif

#if defined(_SC_LEVEL1_DCACHE_LINESIZE)

int main() {
  long val = ::sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
  if (val == -1)
    return 1;
  std::cout << val << std::endl;
  return 0;
}

#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) ||   \
    defined(__DragonFly__) || defined(__APPLE__)

#include <stddef.h> // for size_t
#include <sys/sysctl.h>
#include <sys/types.h>
int main() {
  size_t val = 0;
  size_t size = sizeof(val);
  if (sysctlbyname("hw.cachelinesize", &val, &size, NULL, 0) != 0)
    return 1;
  std::cout << val << std::endl;
  return 0;
}

#elif defined(_WIN32) || defined(_WIN64)

#include <Windows.h>
int main() {
  DWORD buffer_size = 0;
  SYSTEM_LOGICAL_PROCESSOR_INFORMATION* buffer = nullptr;

  GetLogicalProcessorInformation(0, &buffer_size);
  buffer = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION*)malloc(buffer_size);
  GetLogicalProcessorInformation(&buffer[0], &buffer_size);

  for (DWORD i = 0;
       i != buffer_size / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION); ++i) {
    if (buffer[i].Relationship == RelationCache && buffer[i].Cache.Level == 1 &&
        buffer[i].Cache.Type == CacheData) {
      std::cout << buffer[i].Cache.LineSize << std::endl;
      free(buffer);
      return 0;
    }
  }

  free(buffer);
  return 1;
}

#else
#error Do not know how to query CPU L1d cache line size for this system!
#endif
