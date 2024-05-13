#include <cerrno>
#include <cstdint>
#include <iostream>
#include <optional>

#if defined(__unix__)
#include <unistd.h>
#endif

#if defined(__GLIBC__)
#include <elf.h>
#endif

#if defined(_POSIX_C_SOURCE) && defined(_SC_LEVEL1_DCACHE_LINESIZE)
static std::optional<int64_t> get_cachelinesize_from_sysconf() {
  long val = ::sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
  if (val == -1) // On error, -1 is returned.
    return std::nullopt;
  return val;
}
#else
static std::optional<int64_t> get_cachelinesize_from_sysconf() {
  return std::nullopt;
}
#endif

#if defined(__GLIBC__)
#include <sys/auxv.h>
static std::optional<int64_t> get_cachelinesize_from_getauxval() {
  unsigned long geometry = getauxval(AT_L1D_CACHEGEOMETRY);
  if (geometry == 0 && errno == ENOENT) // On error, 0 is returned.
    return std::nullopt;
  geometry &= 0xFFFF; // cache line size in bytes in the bottom 16 bits.
  return geometry;
}
#else
static std::optional<int64_t> get_cachelinesize_from_getauxval() {
  return std::nullopt;
}
#endif

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) ||     \
    defined(__DragonFly__) || defined(__APPLE__)
#include <cstddef>
#include <sys/sysctl.h>
#include <sys/types.h>
static std::optional<int64_t> get_cachelinesize_from_sysctlbyname() {
  int64_t val = 0;
  size_t size = sizeof(val);
  if (sysctlbyname("hw.cachelinesize", &val, &size, NULL, 0) != 0)
    return std::nullopt;
  return val;
}
#else
static std::optional<int64_t> get_cachelinesize_from_sysctlbyname() {
  return std::nullopt;
}
#endif

#if defined(_WIN32) || defined(_WIN64)
#include <cassert>
#include <vector>
//
#include <Windows.h>
static std::optional<int64_t>
get_cachelinesize_from_GetLogicalProcessorInformation() {
  DWORD buffer_size = 0;
  GetLogicalProcessorInformation(nullptr, &buffer_size);
  assert(buffer_size % sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) == 0);
  std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> buffer(
      buffer_size / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));
  GetLogicalProcessorInformation(buffer.data(), &buffer_size);
  for (const auto& e : buffer) {
    if (e.Relationship == RelationCache && e.Cache.Level == 1 &&
        e.Cache.Type == CacheData) {
      return e.Cache.LineSize;
    }
  }
  return std::nullopt;
}
#else
static std::optional<int64_t>
get_cachelinesize_from_GetLogicalProcessorInformation() {
  return std::nullopt;
}
#endif

int main() {
  std::optional<int64_t> val;
  if (!val)
    val = get_cachelinesize_from_sysconf();
  if (!val)
    val = get_cachelinesize_from_getauxval();
  if (!val)
    val = get_cachelinesize_from_sysctlbyname();
  if (!val)
    val = get_cachelinesize_from_GetLogicalProcessorInformation();
#if defined(__riscv)
  if (!val) {
    // On RISC-V, at least on openSUSE TW, at least as of this commit,
    // there is just no way to query this information.
    val = 0; // Pretend we did detect it as zero. Will use fall back value.
  }
#endif
  if (!val) {
    std::cerr
        << "Do not know how to query CPU L1d cache line size for this system!"
        << std::endl;
    return 1;
  }
  std::cout << *val << std::endl;
  return 0;
}
