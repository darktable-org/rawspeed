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

#include "adt/Bit.h"
#include <algorithm>
#include <cassert>
#include <functional>
#include <iterator>
#include <type_traits>
#include <vector>

namespace rawspeed {

template <typename T, int TableBitWidth> class SimpleLUT final {
public:
  using value_type = T;

  SimpleLUT() = default;

private:
  std::vector<value_type> table;

public:
  template <typename F>
    requires(
        !std::is_same_v<SimpleLUT, typename std::remove_cv_t<
                                       typename std::remove_reference_t<F>>> &&
        std::is_convertible_v<
            F, std::function<value_type(typename decltype(table)::size_type,
                                        typename decltype(table)::size_type)>>)
  explicit SimpleLUT(F f) {
    const auto fullTableSize = 1U << TableBitWidth;
    table.reserve(fullTableSize);
    std::generate_n(std::back_inserter(table), fullTableSize,
                    [&f, table_ = &table]() {
                      // which row [0..fullTableSize) are we filling?
                      const auto i = table_->size();
                      return f(i, fullTableSize);
                    });
    assert(table.size() == fullTableSize);
  }

  value_type operator[](int x) const {
    unsigned clampedX = clampBits(x, TableBitWidth);
    return table[clampedX];
  }
};

} // namespace rawspeed
