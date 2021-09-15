/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2018 Stefan Löffler
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

#include "common/Common.h" // for clampBits
#include <algorithm>       // for generate_n
#include <cassert>         // for assert
#include <functional>      // for function
#include <iterator>        // for back_inserter
#include <type_traits>     // for enable_if, is_convertible
#include <vector>          // for vector

namespace rawspeed {

template <typename T, int TableBitWidth> class SimpleLUT final {
public:
  using value_type = T;

  SimpleLUT() = default;

private:
  std::vector<value_type> table;

public:
  template <
      typename F,
      typename = std::enable_if<std::is_convertible_v<
          F, std::function<value_type(typename decltype(table)::size_type,
                                      typename decltype(table)::size_type)>>>>
  explicit SimpleLUT(F&& f) {
    const auto fullTableSize = 1U << TableBitWidth;
    table.reserve(fullTableSize);
    std::generate_n(std::back_inserter(table), fullTableSize,
                    [&f, table = &table]() {
                      // which row [0..fullTableSize) are we filling?
                      const auto i = table->size();
                      return f(i, fullTableSize);
                    });
    assert(table.size() == fullTableSize);
  }

  inline value_type operator[](int x) const {
    unsigned clampedX = clampBits(x, TableBitWidth);
    return table[clampedX];
  }
};

} // namespace rawspeed
