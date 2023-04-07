/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2016-2019 Roman Lebedev

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

#include "rawspeedconfig.h" // for HAVE_OPENMP
#include "common/Common.h"  // for rawspeed_get_number_of_processor_cores

#ifdef HAVE_OPENMP
#include <omp.h> // for omp_get_max_threads
#endif

// define this function, it is only declared in rawspeed:
#ifdef HAVE_OPENMP
extern "C" int __attribute__((visibility("default")))
rawspeed_get_number_of_processor_cores() {
  return omp_get_max_threads();
}
#else
extern "C" int RAWSPEED_READNONE __attribute__((visibility("default")))
rawspeed_get_number_of_processor_cores() {
  return 1;
}
#endif
