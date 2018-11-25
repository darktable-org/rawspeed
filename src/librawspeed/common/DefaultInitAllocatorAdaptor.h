/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2018 Roman Lebedev

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

#include <memory>
#include <type_traits>

namespace rawspeed {

template <typename T, typename ActualAllocator = std::allocator<T>,
          typename = std::enable_if_t<std::is_pod<T>::value>>
class DefaultInitAllocatorAdaptor {
public:
  using allocator_traits = std::allocator_traits<ActualAllocator>;

  using value_type = typename allocator_traits::value_type;
  using pointer = typename allocator_traits::pointer;
  using const_pointer = typename allocator_traits::const_pointer;
  using size_type = typename allocator_traits::size_type;
  using difference_type = typename allocator_traits::difference_type;

  static_assert(std::is_same<T, value_type>::value, "");

  template <class To> struct rebind {
    using other = DefaultInitAllocatorAdaptor<
        To, typename allocator_traits::template rebind_alloc<To>>;
  };

  using allocator_type =
      typename allocator_traits::template rebind_alloc<value_type>;

  allocator_type allocator;

  const allocator_type& get_allocator() const noexcept { return allocator; }

  DefaultInitAllocatorAdaptor() noexcept = default;

  explicit DefaultInitAllocatorAdaptor(
      const allocator_type& allocator_) noexcept
      : allocator(allocator_) {}

  template <class To>
  explicit DefaultInitAllocatorAdaptor(
      const DefaultInitAllocatorAdaptor<
          To, typename allocator_traits::template rebind_alloc<To>>&
          allocator_) noexcept
      : allocator(allocator_.get_allocator()) {}

  pointer allocate(size_type n, const void* hint = nullptr) {
    return allocator.allocate(n, hint);
  }

  void deallocate(pointer p, size_type n) noexcept {
    allocator.deallocate(p, n);
  }

  template <typename U>
  void
  construct(U* ptr) noexcept(std::is_nothrow_default_constructible<U>::value) {
    ::new (static_cast<void*>(ptr)) U; // start the life-time, but do not init.
  }

  using propagate_on_container_copy_assignment =
      typename allocator_traits::propagate_on_container_copy_assignment;
  using propagate_on_container_move_assignment =
      typename allocator_traits::propagate_on_container_move_assignment;
  using propagate_on_container_swap =
      typename allocator_traits::propagate_on_container_swap;
};

template <typename T0, typename A0, typename T1, typename A1>
bool operator==(DefaultInitAllocatorAdaptor<T0, A0> const& x,
                DefaultInitAllocatorAdaptor<T1, A1> const& y) noexcept {
  return x.get_allocator() == y.get_allocator();
}

template <typename T0, typename A0, typename T1, typename A1>
bool operator!=(DefaultInitAllocatorAdaptor<T0, A0> const& x,
                DefaultInitAllocatorAdaptor<T1, A1> const& y) noexcept {
  return !(x == y);
}

} // namespace rawspeed
