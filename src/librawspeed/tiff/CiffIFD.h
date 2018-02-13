/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2014 Pedro Côrte-Real
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

#include "common/Common.h"      // for uint32
#include "common/NORangesSet.h" // for NORangesSet
#include "tiff/CiffEntry.h"     // IWYU pragma: keep
#include "tiff/CiffTag.h"       // for CiffTag
#include <map>                  // for map
#include <memory>               // for unique_ptr
#include <string>               // for string
#include <vector>               // for vector

namespace rawspeed {

class ByteStream;

class CiffIFD final {
  CiffIFD* const parent;

  std::vector<std::unique_ptr<const CiffIFD>> mSubIFD;
  std::map<CiffTag, std::unique_ptr<const CiffEntry>> mEntry;

  int subIFDCount = 0;
  int subIFDCountRecursive = 0;

  void recursivelyIncrementSubIFDCount();
  void checkSubIFDs(int headroom) const;
  void recursivelyCheckSubIFDs(int headroom) const;

  // CIFF IFD are tree-like structure, with branches.
  // A branch (IFD) can have branches (IFDs) of it's own.
  // We must be careful to weed-out all the degenerative cases that
  // can be produced e.g. via fuzzing, or other means.
  struct Limits final {
    // How many layers of IFD's can there be?
    // All RPU samples (as of 2018-02-13) are ok with 3.
    // However, let's be on the safe side, and pad it by one.
    static constexpr int Depth = 3 + 1;

    // How many sub-IFD's can this IFD have?
    // NOTE: only for the given IFD, *NOT* recursively including all sub-IFD's!
    // All RPU samples (as of 2018-02-13) are ok with 4.
    // However, let's be on the safe side, and double it.
    static constexpr int SubIFDCount = 4 * 2;

    // How many sub-IFD's can this IFD have, recursively?
    // All RPU samples (as of 2018-02-13) are ok with 6.
    // However, let's be on the safe side, and double it.
    static constexpr int RecursiveSubIFDCount = 6 * 2;
  };

  void add(std::unique_ptr<CiffIFD> subIFD);
  void add(std::unique_ptr<CiffEntry> entry);

  void parseIFDEntry(NORangesSet<Buffer>* valueDatas,
                     const ByteStream* valueData, ByteStream* dirEntries);

  template <typename Lambda>
  std::vector<const CiffIFD*> __attribute__((pure))
  getIFDsWithTagIf(CiffTag tag, const Lambda& f) const;

  template <typename Lambda>
  const CiffEntry* __attribute__((pure))
  getEntryRecursiveIf(CiffTag tag, const Lambda& f) const;

public:
  explicit CiffIFD(CiffIFD* parent);
  CiffIFD(CiffIFD* parent, ByteStream directory);

  std::vector<const CiffIFD*> __attribute__((pure))
  getIFDsWithTag(CiffTag tag) const;
  std::vector<const CiffIFD*> __attribute__((pure))
  getIFDsWithTagWhere(CiffTag tag, uint32 isValue) const;
  std::vector<const CiffIFD*> __attribute__((pure))
  getIFDsWithTagWhere(CiffTag tag, const std::string& isValue) const;

  bool __attribute__((pure)) hasEntry(CiffTag tag) const;
  bool __attribute__((pure)) hasEntryRecursive(CiffTag tag) const;

  const CiffEntry* __attribute__((pure)) getEntry(CiffTag tag) const;
  const CiffEntry* __attribute__((pure)) getEntryRecursive(CiffTag tag) const;
  const CiffEntry* __attribute__((pure))
  getEntryRecursiveWhere(CiffTag tag, uint32 isValue) const;
  const CiffEntry* __attribute__((pure))
  getEntryRecursiveWhere(CiffTag tag, const std::string& isValue) const;
};

} // namespace rawspeed
