#include <cstddef>
#include <iostream>

#if defined(__i386__) || defined(__x86_64__)

#include <cpuid.h>

/* Features in %edx for leaf 1 */
#if !defined(bit_PSE)
#define bit_PSE 0x00000008
#endif
#if !defined(bit_PAE)
#define bit_PAE 0x00000040
#endif

int main() {
  unsigned int eax;
  unsigned int ebx;
  unsigned int ecx;
  unsigned int edx;

  if (!__get_cpuid(1, &eax, &ebx, &ecx, &edx))
    return 1; // Detection failed.

  size_t val;
  if (edx & bit_PAE)
    val = 2 * 1024 * 1024; // 2 MiB
  else if (edx & bit_PSE)
    val = 4 * 1024 * 1024; // 4 MiB
  else
    val = 4 * 1024; // 4 KiB

  std::cout << val << std::endl;
  return 0;
}

#else

int main() {
  // Don't know how to perform detection. Just fall back to page size.
  std::cout << RAWSPEED_PAGESIZE << std::endl;
  return 0;
}

#endif
