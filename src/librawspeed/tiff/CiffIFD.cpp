/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2014 Pedro Côrte-Real
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

#include "tiff/CiffIFD.h"
#include "common/Common.h"               // for uint32, ushort16
#include "common/RawspeedException.h"    // for RawspeedException
#include "io/ByteStream.h"               // for ByteStream
#include "io/IOException.h"              // for IOException
#include "parsers/CiffParserException.h" // for ThrowCPE, CiffParserException
#include "tiff/CiffEntry.h"              // for CiffEntry, CiffDataType::CI...
#include <map>                           // for map, _Rb_tree_iterator
#include <memory>                        // for unique_ptr
#include <string>                        // for allocator, operator==, string
#include <utility>                       // for pair
#include <vector>                        // for vector

using std::string;
using std::vector;
using std::unique_ptr;

namespace rawspeed {

void CiffIFD::parseIFDEntry(ByteStream* bs) {
  unique_ptr<CiffEntry> t;

  auto origPos = bs->getPosition();

  try {
    t = make_unique<CiffEntry>(bs);
  } catch (IOException&) {
    // Ignore unparsable entry, but fix probably broken position due to
    // interruption by exception; i.e. setting it to the next entry.
    bs->setPosition(origPos + 10);
    return;
  }

  try {
    switch (t->type) {
    case CIFF_SUB1:
    case CIFF_SUB2: {
      add(make_unique<CiffIFD>(this, &t->data));
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

CiffIFD::CiffIFD(const CiffIFD* parent_, ByteStream* mFile) : parent(parent_) {
  checkOverflow();

  if (mFile->getSize() < 4)
    ThrowCPE("File is probably corrupted.");

  mFile->setPosition(mFile->getSize() - 4);
  uint32 valuedata_size = mFile->getU32();

  mFile->setPosition(valuedata_size);
  ushort16 dircount = mFile->getU16();

  for (uint32 i = 0; i < dircount; i++)
    parseIFDEntry(mFile);
}

void CiffIFD::checkOverflow() const {
  const CiffIFD* p = this;
  int i = 0;
  while ((p = p->parent) != nullptr) {
    i++;
    if (i > 5)
      ThrowCPE("CiffIFD cascading overflow.");
  }
}

void CiffIFD::add(std::unique_ptr<CiffIFD> subIFD) {
  if (mSubIFD.size() > 100)
    ThrowCPE("CIFF file has too many SubIFDs, probably broken");

  subIFD->parent = this;
  mSubIFD.push_back(move(subIFD));
}

void CiffIFD::add(std::unique_ptr<CiffEntry> entry) {
  entry->parent = this;
  mEntry[entry->tag] = move(entry);
}

template <typename Lambda>
std::vector<const CiffIFD*> CiffIFD::getIFDsWithTagIf(CiffTag tag,
                                                      const Lambda& f) const {
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
  return getIFDsWithTagIf(tag, [](const CiffEntry*) { return true; });
}

vector<const CiffIFD*> CiffIFD::getIFDsWithTagWhere(CiffTag tag,
                                                    uint32 isValue) const {
  return getIFDsWithTagIf(tag, [&isValue](const CiffEntry* entry) {
    return entry->isInt() && entry->getU32() == isValue;
  });
}

vector<const CiffIFD*>
CiffIFD::getIFDsWithTagWhere(CiffTag tag, const string& isValue) const {
  return getIFDsWithTagIf(tag, [&isValue](const CiffEntry* entry) {
    return entry->isString() && isValue == entry->getString();
  });
}

bool __attribute__((pure)) CiffIFD::hasEntry(CiffTag tag) const {
  return mEntry.count(tag) > 0;
}

bool __attribute__((pure)) CiffIFD::hasEntryRecursive(CiffTag tag) const {
  if (mEntry.count(tag) > 0)
    return true;

  for (const auto& i : mSubIFD) {
    if (i->hasEntryRecursive(tag))
      return true;
  }

  return false;
}

const CiffEntry* CiffIFD::getEntry(CiffTag tag) const {
  const auto found = mEntry.find(tag);
  if (found != mEntry.end())
    return found->second.get();

  ThrowCPE("Entry 0x%x not found.", tag);
}

const CiffEntry* CiffIFD::getEntryRecursive(CiffTag tag) const {
  return getEntryRecursiveIf(tag, [](const CiffEntry*) { return true; });
}

const CiffEntry* CiffIFD::getEntryRecursiveWhere(CiffTag tag,
                                                 uint32 isValue) const {
  return getEntryRecursiveIf(tag, [&isValue](const CiffEntry* entry) {
    return entry->isInt() && entry->getU32() == isValue;
  });
}

const CiffEntry* CiffIFD::getEntryRecursiveWhere(CiffTag tag,
                                                 const string& isValue) const {
  return getEntryRecursiveIf(tag, [&isValue](const CiffEntry* entry) {
    return entry->isString() && isValue == entry->getString();
  });
}

} // namespace rawspeed
