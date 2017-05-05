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

#ifdef HAVE_PTHREAD
#include <pthread.h>
#endif

// see https://clang.llvm.org/docs/ThreadSafetyAnalysis.html#mutexheader

// Enable thread safety attributes only with clang.
// The attributes can be safely erased when compiling with other compilers.
#if defined(__clang__) && (!defined(SWIG))
#define RS_THREAD_ANNOTATION_ATTRIBUTE(x) __attribute__((x))
#else
#define RS_THREAD_ANNOTATION_ATTRIBUTE(x) // no-op
#endif

#define CAPABILITY(x) RS_THREAD_ANNOTATION_ATTRIBUTE(capability(x))

#define SCOPED_CAPABILITY RS_THREAD_ANNOTATION_ATTRIBUTE(scoped_lockable)

#define GUARDED_BY(x) RS_THREAD_ANNOTATION_ATTRIBUTE(guarded_by(x))

#define PT_GUARDED_BY(x) RS_THREAD_ANNOTATION_ATTRIBUTE(pt_guarded_by(x))

#define ACQUIRED_BEFORE(...)                                                   \
  RS_THREAD_ANNOTATION_ATTRIBUTE(acquired_before(__VA_ARGS__))

#define ACQUIRED_AFTER(...)                                                    \
  RS_THREAD_ANNOTATION_ATTRIBUTE(acquired_after(__VA_ARGS__))

#define REQUIRES(...)                                                          \
  RS_THREAD_ANNOTATION_ATTRIBUTE(requires_capability(__VA_ARGS__))

#define REQUIRES_SHARED(...)                                                   \
  RS_THREAD_ANNOTATION_ATTRIBUTE(requires_shared_capability(__VA_ARGS__))

#define ACQUIRE(...)                                                           \
  RS_THREAD_ANNOTATION_ATTRIBUTE(acquire_capability(__VA_ARGS__))

#define ACQUIRE_SHARED(...)                                                    \
  RS_THREAD_ANNOTATION_ATTRIBUTE(acquire_shared_capability(__VA_ARGS__))

#define RELEASE(...)                                                           \
  RS_THREAD_ANNOTATION_ATTRIBUTE(release_capability(__VA_ARGS__))

#define RELEASE_SHARED(...)                                                    \
  RS_THREAD_ANNOTATION_ATTRIBUTE(release_shared_capability(__VA_ARGS__))

#define TRY_ACQUIRE(...)                                                       \
  RS_THREAD_ANNOTATION_ATTRIBUTE(try_acquire_capability(__VA_ARGS__))

#define TRY_ACQUIRE_SHARED(...)                                                \
  RS_THREAD_ANNOTATION_ATTRIBUTE(try_acquire_shared_capability(__VA_ARGS__))

#define EXCLUDES(...)                                                          \
  RS_THREAD_ANNOTATION_ATTRIBUTE(locks_excluded(__VA_ARGS__))

#define ASSERT_CAPABILITY(x)                                                   \
  RS_THREAD_ANNOTATION_ATTRIBUTE(assert_capability(x))

#define ASSERT_SHARED_CAPABILITY(x)                                            \
  RS_THREAD_ANNOTATION_ATTRIBUTE(assert_shared_capability(x))

#define RETURN_CAPABILITY(x) RS_THREAD_ANNOTATION_ATTRIBUTE(lock_returned(x))

#define NO_THREAD_SAFETY_ANALYSIS                                              \
  RS_THREAD_ANNOTATION_ATTRIBUTE(no_thread_safety_analysis)

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
