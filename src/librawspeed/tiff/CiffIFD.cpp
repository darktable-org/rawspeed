/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2014 Pedro Côrte-Real
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

#include "tiff/CiffIFD.h"
#include "common/Common.h"               // for uint32, ushort16, isIn
#include "common/NORangesSet.h"          // for NORangesSet
#include "common/RawspeedException.h"    // for RawspeedException
#include "io/ByteStream.h"               // for ByteStream
#include "io/IOException.h"              // for IOException
#include "parsers/CiffParserException.h" // for ThrowCPE, CiffParserException
#include "tiff/CiffEntry.h"              // for CiffEntry, CiffDataType::CI...
#include <cassert>                       // for assert
#include <map>                           // for map, _Rb_tree_iterator
#include <memory>                        // for unique_ptr
#include <string>                        // for allocator, operator==, string
#include <utility>                       // for pair
#include <vector>                        // for vector

using std::string;
using std::vector;
using std::unique_ptr;

namespace rawspeed {

void CiffIFD::parseIFDEntry(NORangesSet<Buffer>* valueDatas,
                            const ByteStream* valueData,
                            ByteStream* dirEntries) {
  assert(valueDatas);
  assert(valueData);
  assert(dirEntries);

  unique_ptr<CiffEntry> t;
  ByteStream dirEntry = dirEntries->getStream(10); // Entry is 10 bytes.

  try {
    t = std::make_unique<CiffEntry>(valueDatas, valueData, dirEntry);
  } catch (IOException&) {
    // Ignore unparsable entry
    return;
  }

  switch (t->type) {
  case CIFF_SUB1:
  case CIFF_SUB2: {
    // Ok, have to store it.
    break;
  }

  default:
    // We will never look for this entry. No point in storing it.
    if (!isIn(t->tag, CiffTagsWeCareAbout))
      return;
  }

  try {
    switch (t->type) {
    case CIFF_SUB1:
    case CIFF_SUB2: {
      add(std::make_unique<CiffIFD>(this, &t->data));
      break;
    }

    default:
      add(move(t));
    }
  } catch (RawspeedException&) {
    // Unparsable private data are added as entries
    add(move(t));
  }
}

CiffIFD::CiffIFD(CiffIFD* const parent_) : parent(parent_) {
  recursivelyCheckSubIFDs(1);
  // If we are good (can add this IFD without violating the limits),
  // we are still here. However, due to the way we add parsed sub-IFD's (lazy),
  // we need to count this IFD right *NOW*, not when adding it at the end.
  recursivelyIncrementSubIFDCount();
}

CiffIFD::CiffIFD(CiffIFD* const parent_, ByteStream* directory)
    : CiffIFD(parent_) {
  assert(directory);

  if (directory->getSize() < 4)
    ThrowCPE("File is probably corrupted.");

  directory->setPosition(directory->getSize() - 4);
  const uint32 valueDataSize = directory->getU32();

  // The Recursion. Directory entries store data here. May contain IFDs.
  directory->setPosition(0);
  const ByteStream valueData(directory->getStream(valueDataSize));

  // count of the Directory entries in this IFD
  const ushort16 entryCount = directory->getU16();

  // each entry is 10 bytes
  ByteStream dirEntries(directory->getStream(entryCount, 10));

  // IFDData might still contain OtherData until the valueDataSize at the end.
  // But we do not care about that.

  // Each IFD has it's own valueData area.
  // In that area, no two entries may overlap.
  NORangesSet<Buffer> valueDatas;

  for (uint32 i = 0; i < entryCount; i++)
    parseIFDEntry(&valueDatas, &valueData, &dirEntries);
}

void CiffIFD::recursivelyIncrementSubIFDCount() {
  CiffIFD* p = this->parent;
  if (!p)
    return;

  p->subIFDCount++;

  for (; p != nullptr; p = p->parent)
    p->subIFDCountRecursive++;
}

void CiffIFD::checkSubIFDs(int headroom) const {
  int count = headroom + subIFDCount;
  if (!headroom)
    assert(count <= CiffIFD::Limits::SubIFDCount);
  else if (count > CiffIFD::Limits::SubIFDCount)
    ThrowCPE("TIFF IFD has %u SubIFDs", count);

  count = headroom + subIFDCountRecursive;
  if (!headroom)
    assert(count <= CiffIFD::Limits::RecursiveSubIFDCount);
  else if (count > CiffIFD::Limits::RecursiveSubIFDCount)
    ThrowCPE("TIFF IFD file has %u SubIFDs (recursively)", count);
}

void CiffIFD::recursivelyCheckSubIFDs(int headroom) const {
  int depth = 0;
  for (const CiffIFD* p = this; p != nullptr;) {
    if (!headroom)
      assert(depth <= CiffIFD::Limits::Depth);
    else if (depth > CiffIFD::Limits::Depth)
      ThrowCPE("CiffIFD cascading overflow, found %u level IFD", depth);

    p->checkSubIFDs(headroom);

    // And step up
    p = p->parent;
    depth++;
  }
}

void CiffIFD::add(std::unique_ptr<CiffIFD> subIFD) {
  assert(subIFD->parent == this);

  // We are good, and actually can add this sub-IFD, right?
  subIFD->recursivelyCheckSubIFDs(0);

  mSubIFD.push_back(move(subIFD));
}

void CiffIFD::add(std::unique_ptr<CiffEntry> entry) {
  assert(isIn(entry->tag, CiffTagsWeCareAbout));
  mEntry[entry->tag] = move(entry);
  assert(mEntry.size() <= CiffTagsWeCareAbout.size());
}

template <typename Lambda>
std::vector<const CiffIFD*> CiffIFD::getIFDsWithTagIf(CiffTag tag,
                                                      const Lambda& f) const {
  assert(isIn(tag, CiffTagsWeCareAbout));

  std::vector<const CiffIFD*> matchingIFDs;

  const auto found = mEntry.find(tag);
  if (found != mEntry.end()) {
    const auto entry = found->second.get();
    if (f(entry))
      matchingIFDs.push_back(this);
  }

  for (const auto& i : mSubIFD) {
    const auto t = i->getIFDsWithTagIf(tag, f);
    matchingIFDs.insert(matchingIFDs.end(), t.begin(), t.end());
  }

  return matchingIFDs;
}

template <typename Lambda>
const CiffEntry* CiffIFD::getEntryRecursiveIf(CiffTag tag,
                                              const Lambda& f) const {
  assert(isIn(tag, CiffTagsWeCareAbout));

  const auto found = mEntry.find(tag);
  if (found != mEntry.end()) {
    const auto entry = found->second.get();
    if (f(entry))
      return entry;
  }

  for (const auto& i : mSubIFD) {
    const CiffEntry* entry = i->getEntryRecursiveIf(tag, f);
    if (entry)
      return entry;
  }

  return nullptr;
}

vector<const CiffIFD*> CiffIFD::getIFDsWithTag(CiffTag tag) const {
  assert(isIn(tag, CiffTagsWeCareAbout));
  return getIFDsWithTagIf(tag, [](const CiffEntry*) { return true; });
}

vector<const CiffIFD*> CiffIFD::getIFDsWithTagWhere(CiffTag tag,
                                                    uint32 isValue) const {
  assert(isIn(tag, CiffTagsWeCareAbout));
  return getIFDsWithTagIf(tag, [&isValue](const CiffEntry* entry) {
    return entry->isInt() && entry->getU32() == isValue;
  });
}

vector<const CiffIFD*>
CiffIFD::getIFDsWithTagWhere(CiffTag tag, const string& isValue) const {
  assert(isIn(tag, CiffTagsWeCareAbout));
  return getIFDsWithTagIf(tag, [&isValue](const CiffEntry* entry) {
    return entry->isString() && isValue == entry->getString();
  });
}

bool __attribute__((pure)) CiffIFD::hasEntry(CiffTag tag) const {
  assert(isIn(tag, CiffTagsWeCareAbout));

  return mEntry.count(tag) > 0;
}

bool __attribute__((pure)) CiffIFD::hasEntryRecursive(CiffTag tag) const {
  assert(isIn(tag, CiffTagsWeCareAbout));

  if (mEntry.count(tag) > 0)
    return true;

  for (const auto& i : mSubIFD) {
    if (i->hasEntryRecursive(tag))
      return true;
  }

  return false;
}

const CiffEntry* CiffIFD::getEntry(CiffTag tag) const {
  assert(isIn(tag, CiffTagsWeCareAbout));

  const auto found = mEntry.find(tag);
  if (found != mEntry.end())
    return found->second.get();

  ThrowCPE("Entry 0x%x not found.", tag);
}

const CiffEntry* CiffIFD::getEntryRecursive(CiffTag tag) const {
  assert(isIn(tag, CiffTagsWeCareAbout));
  return getEntryRecursiveIf(tag, [](const CiffEntry*) { return true; });
}

const CiffEntry* CiffIFD::getEntryRecursiveWhere(CiffTag tag,
                                                 uint32 isValue) const {
  assert(isIn(tag, CiffTagsWeCareAbout));
  return getEntryRecursiveIf(tag, [&isValue](const CiffEntry* entry) {
    return entry->isInt() && entry->getU32() == isValue;
  });
}

const CiffEntry* CiffIFD::getEntryRecursiveWhere(CiffTag tag,
                                                 const string& isValue) const {
  assert(isIn(tag, CiffTagsWeCareAbout));
  return getEntryRecursiveIf(tag, [&isValue](const CiffEntry* entry) {
    return entry->isString() && isValue == entry->getString();
  });
}

} // namespace rawspeed
