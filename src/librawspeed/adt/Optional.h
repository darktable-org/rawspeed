/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2024 Roman Lebedev

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
#include "adt/Invariant.h"
#include <concepts>
#include <optional> // IWYU pragma: export

namespace rawspeed {

template <typename T> class Optional final {
  // NOLINTNEXTLINE(rawspeed-no-std-optional): we're in the wrapper.
  std::optional<T> impl;

public:
  Optional() = default;

  template <typename U = T>
    requires(!std::same_as<U, Optional<T>> && !std::same_as<U, Optional<T>&> &&
             !std::same_as<U, Optional<T> &&> &&
             !std::same_as<U, std::optional<T>>)
  // NOLINTNEXTLINE(google-explicit-constructor)
  Optional(U&& value) : impl(std::forward<U>(value)) {}

  template <typename U = T>
    requires(std::same_as<U, T>)
  Optional<T>& operator=(U&& value) {
    impl = std::forward<U>(value);
    return *this;
  }

  template <typename... Args> T& emplace(Args&&... args) {
    return impl.emplace(std::forward<Args>(args)...);
  }

  const T* operator->() const {
    invariant(has_value());
    return &impl.value();
  }

  T* operator->() {
    invariant(has_value());
    return &impl.value();
  }

  const T& operator*() const& {
    invariant(has_value());
    return impl.value();
  }

  T& operator*() & {
    invariant(has_value());
    return impl.value();
  }

  T&& operator*() && {
    invariant(has_value());
    return std::move(impl.value());
  }

  const T&& operator*() const&& {
    invariant(has_value());
    return std::move(impl.value());
  }

  [[nodiscard]] bool has_value() const RAWSPEED_READNONE {
    return impl.has_value();
  }

  explicit operator bool() const { return has_value(); }

  template <typename U> T value_or(U&& fallback) const& {
    return impl.value_or(std::forward<U>(fallback));
  }

  void reset() { impl.reset(); }
};

template <typename T, typename U>
bool operator==(const Optional<T>& lhs, const U& rhs) {
  return lhs && *lhs == rhs;
}

template <typename T, typename U>
bool operator==(const Optional<T>& lhs, const Optional<U>& rhs) {
  if (lhs.has_value() != rhs.has_value())
    return false;
  if (!lhs.has_value())
    return true;
  return *lhs == *rhs;
}

} // namespace rawspeed
