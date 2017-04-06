/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2017 Roman Lebedev

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include "common/Cpuid.h"

namespace rawspeed {

#if defined(__i386__) || defined(__x86_64__)

#include <cpuid.h> // for __get_cpuid, bit_SSE2

bool Cpuid::SSE2() {
  unsigned int eax, ebx, ecx, edx;

  if (!__get_cpuid(1, &eax, &ebx, &ecx, &edx))
    return false;

  return edx & bit_SSE2;
}

#else

bool Cpuid::SSE2() { return false; }

#endif

} // namespace rawspeed
