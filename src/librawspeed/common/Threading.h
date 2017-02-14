/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2017 Roman Lebedev
    Copyright (C) 2017 Axel Waggershauser

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

#ifdef NO_PTHREAD

#define pthread_mutex_init(A, B)
#define pthread_mutex_destroy(A)
#define pthread_mutex_lock(A)
#define pthread_mutex_unlock(A)

#else

#include <pthread.h> // IWYU pragma: export

#endif

#include "common/Common.h"
#include <vector>
#include <memory>

namespace RawSpeed {

template <typename T>
class ThreadSafeVector : protected std::vector<T>
{
#ifdef NO_PTHREAD
  struct pthread_mutext_t;
#endif

  // use a pimpl idiom here to make sure the stack size of this class is the
  // same with or without NO_THREAD, so we get a stable ABI.
  //TODO: replace with std::mutex
  std::unique_ptr<pthread_mutex_t> mutex;

  using base_t = std::vector<T>;

public:
  ThreadSafeVector()
  {
    mutex = make_unique<pthread_mutex_t>();
    pthread_mutex_init(mutex.get(), nullptr);
  }

  ~ThreadSafeVector()
  {
    pthread_mutex_destroy(mutex.get());
  }

  void push_back(T v)
  {
    pthread_mutex_lock(mutex.get());
    base_t::push_back(v);
    pthread_mutex_unlock(mutex.get());
  }

  template <typename Container> void append(const Container& c)
  {
    pthread_mutex_lock(mutex.get());
    base_t::insert(end(), c.begin(), c.end());
    pthread_mutex_unlock(mutex.get());
  }

  void clear()
  {
    pthread_mutex_lock(mutex.get());
    base_t::clear();
    pthread_mutex_unlock(mutex.get());
  }

  using base_t::end;
  using base_t::begin;
  using base_t::empty;
  using base_t::size;
  using base_t::operator[];
};

}
