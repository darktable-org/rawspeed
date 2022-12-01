/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
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

#include "common/TableLookUp.h"
#include "common/Common.h"                // for clampBits
#include "decoders/RawDecoderException.h" // for ThrowException, ThrowRDE
#include <cassert>                        // for assert
#include <cstdint>                        // for uint16_t
#include <limits>                         // for numeric_limits

namespace rawspeed {

// How many different values can uint16_t represent?
constexpr int TABLE_MAX_ELTS = std::numeric_limits<uint16_t>::max() + 1;
constexpr int TABLE_SIZE = TABLE_MAX_ELTS * 2;

// Creates n numre of tables.
TableLookUp::TableLookUp(int _ntables, bool _dither)
    : ntables(_ntables), dither(_dither) {
  if (ntables < 1) {
    ThrowRDE("Cannot construct 0 tables");
  }
  tables.resize(ntables * TABLE_SIZE, uint16_t(0));
}

void TableLookUp::setTable(int ntable, const std::vector<uint16_t>& table) {
  assert(!table.empty());

  const int nfilled = table.size();
  if (nfilled > TABLE_MAX_ELTS)
    ThrowRDE("Table lookup with %i entries is unsupported", nfilled);

  if (ntable > ntables) {
    ThrowRDE("Table lookup with number greater than number of tables.");
  }
  uint16_t* t = &tables[ntable * TABLE_SIZE];
  if (!dither) {
    for (int i = 0; i < TABLE_MAX_ELTS; i++) {
      t[i] = (i < nfilled) ? table[i] : table[nfilled - 1];
    }
    return;
  }
  for (int i = 0; i < nfilled; i++) {
    int center = table[i];
    int lower = i > 0 ? table[i - 1] : center;
    int upper = i < (nfilled - 1) ? table[i + 1] : center;
    int delta = upper - lower;
    t[i * 2] = clampBits(center - ((upper - lower + 2) / 4), 16);
    t[i * 2 + 1] = uint16_t(delta);
    // FIXME: this is completely broken when the curve is non-monotonic.
  }

  for (int i = nfilled; i < TABLE_MAX_ELTS; i++) {
    t[i * 2] = table[nfilled - 1];
    t[i * 2 + 1] = 0;
  }
  t[0] = t[1];
  t[TABLE_SIZE - 1] = t[TABLE_SIZE - 2];
}

uint16_t* TableLookUp::getTable(int n) {
  if (n > ntables) {
    ThrowRDE("Table lookup with number greater than number of tables.");
  }
  return &tables[n * TABLE_SIZE];
}

} // namespace rawspeed
