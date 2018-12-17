/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2017-2018 Roman Lebedev

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

#include "rawspeedconfig.h"
#include "ThreadSafetyAnalysis.h"

#ifdef HAVE_OPENMP
#include <omp.h>
#endif

namespace rawspeed {

// Defines an annotated interface for mutexes.
// These methods can be implemented to use any internal mutex implementation.
#ifdef HAVE_OPENMP

class CAPABILITY("mutex") Mutex final {
  omp_lock_t mutex;

public:
  explicit Mutex() { omp_init_lock(&mutex); }

  ~Mutex() { omp_destroy_lock(&mutex); }

  // Acquire/lock this mutex exclusively.  Only one thread can have exclusive
  // access at any one time.  Write operations to guarded data require an
  // exclusive lock.
  void Lock() ACQUIRE() { omp_set_lock(&mutex); }

  // Release/unlock an exclusive mutex.
  void Unlock() RELEASE() { omp_unset_lock(&mutex); }

  // Try to acquire the mutex.  Returns true on success, and false on failure.
  bool TryLock() TRY_ACQUIRE(true) { return omp_test_lock(&mutex); }

  // For negative capabilities.
  const Mutex& operator!() const { return *this; }
};

#else

class CAPABILITY("mutex") Mutex final {
public:
  // Acquire/lock this mutex exclusively.  Only one thread can have exclusive
  // access at any one time.  Write operations to guarded data require an
  // exclusive lock.
  void __attribute__((const)) Lock() const ACQUIRE() {
    // NOP, since there is no mutex. only here to still check for proper locking
  }

  // Release/unlock an exclusive mutex.
  void __attribute__((const)) Unlock() const RELEASE() {
    // NOP, since there is no mutex. only here to still check for proper locking
  }

  // Try to acquire the mutex.  Returns true on success, and false on failure.
  bool __attribute__((const)) TryLock() const TRY_ACQUIRE(true) {
    // NOP, since there is no mutex. only here to still check for proper locking
    return true;
  }

  // For negative capabilities.
  const Mutex& operator!() const { return *this; }
};

#endif

// MutexLocker is an RAII class that acquires a mutex in its constructor, and
// releases it in its destructor.
class SCOPED_CAPABILITY MutexLocker final {
  Mutex* mut;

public:
  explicit MutexLocker(Mutex* mu) ACQUIRE(mu) : mut(mu) { mu->Lock(); }
  ~MutexLocker() RELEASE() { mut->Unlock(); }
};

} // namespace rawspeed
