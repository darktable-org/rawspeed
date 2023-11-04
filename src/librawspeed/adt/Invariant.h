/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2023 Roman Lebedev

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

#ifndef NDEBUG

#include <cassert>

#define invariant(expr) assert(expr)

#else // NDEBUG

#ifndef __has_builtin      // Optional of course.
#define __has_builtin(x) 0 // Compatibility with non-clang compilers.
#endif

#if __has_builtin(__builtin_assume)

#define invariant(expr) __builtin_assume(expr)

#else // __has_builtin(__builtin_assume)

namespace rawspeed {

__attribute__((always_inline)) constexpr inline void invariant(bool precond) {
  if (!precond)
    __builtin_unreachable();
}

} // namespace rawspeed

#endif // __has_builtin(__builtin_assume)

#endif // NDEBUG
