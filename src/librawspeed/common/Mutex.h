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

#include "rawspeedconfig.h"
#include "ThreadSafetyAnalysis.h"

#ifdef HAVE_PTHREAD
#include <pthread.h>
#endif

namespace rawspeed {

// Defines an annotated interface for mutexes.
// These methods can be implemented to use any internal mutex implementation.
#ifdef HAVE_PTHREAD

class CAPABILITY("mutex") Mutex final {
  pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

public:
  ~Mutex() { pthread_mutex_destroy(&mutex); }

  // Acquire/lock this mutex exclusively.  Only one thread can have exclusive
  // access at any one time.  Write operations to guarded data require an
  // exclusive lock.
  void Lock() ACQUIRE() { pthread_mutex_lock(&mutex); }

  // Release/unlock an exclusive mutex.
  void Unlock() RELEASE() { pthread_mutex_unlock(&mutex); }

  // Try to acquire the mutex.  Returns true on success, and false on failure.
  bool TryLock() TRY_ACQUIRE(true) {
    return pthread_mutex_trylock(&mutex) == 0;
  }

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
