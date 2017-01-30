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

#pragma once

#include "common/Common.h" // for isPowerOfTwo
#include <cstddef>         // for size_t
#include <cstdint>         // for SIZE_MAX, uintptr_t

namespace RawSpeed {

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-attributes"
#pragma GCC diagnostic ignored "-Wattributes"

// coverity[+alloc]
void* alignedMalloc(size_t size, size_t alignment)
    __attribute__((malloc, warn_unused_result, alloc_size(1), alloc_align(2)));

#pragma GCC diagnostic pop

// coverity[+free : arg-0]
void alignedFree(void* ptr);

} // namespace RawSpeed
