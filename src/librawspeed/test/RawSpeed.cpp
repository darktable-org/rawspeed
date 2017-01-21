/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2016 Roman Lebedev

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

#include "common/StdAfx.h"
#include <gmock/gmock.h> // for InitGoogleTest, RUN_ALL_TESTS
#include <iostream>      // for operator<<, basic_ostream, basic...

#ifdef _OPENMP
#include <omp.h>
#endif

using namespace RawSpeed;

// define this function, it is only declared in rawspeed:
int rawspeed_get_number_of_processor_cores() {
#ifdef _OPENMP
  return omp_get_num_procs();
#else
  return 1;
#endif
}
