#ifndef TIFF_IFD_H
#define TIFF_IFD_H

#include "Buffer.h"
#include "TiffEntry.h"
#include "TiffParserException.h"

/* 
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
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

    http://www.klauspost.com
*/

namespace RawSpeed {

class TiffIFD;
class TiffRootIFD;
using TiffIFDOwner = std::unique_ptr<TiffIFD>;
using TiffRootIFDOwner = std::unique_ptr<TiffRootIFD>;
using TiffEntryOwner = std::unique_ptr<TiffEntry>;

class TiffIFD
{
  uint32 nextIFD = 0;
  TiffIFD* parent = nullptr;
  vector<TiffIFDOwner> subIFDs;
  map<TiffTag, TiffEntryOwner> entries;

  friend class TiffEntry;
  friend class RawParser;
  friend TiffRootIFDOwner parseTiff(const Buffer& data);

  // make sure we never copy-constuct/assign a TiffIFD to keep the owning subcontainers contents save
  TiffIFD(const TiffIFD&) = delete;
  TiffIFD& operator=(const TiffIFD&) = delete;

  void add(TiffIFDOwner subIFD);
  void add(TiffEntryOwner entry);
  TiffRootIFDOwner parseDngPrivateData(TiffEntry *t);
  TiffRootIFDOwner parseMakerNote(TiffEntry *t);

public:
  TiffIFD() {}
  TiffIFD(const DataBuffer& data, uint32 offset, TiffIFD* parent);
  virtual ~TiffIFD() {}
  uint32 getNextIFD() const {return nextIFD;}
  //TODO: make public api totally const
  vector<TiffIFD*> getIFDsWithTag(TiffTag tag);
  TiffEntry* getEntry(TiffTag tag) const;
  TiffEntry* getEntryRecursive(TiffTag tag) const;
  bool hasEntry(TiffTag tag) const { return entries.find(tag) != entries.end(); }
  bool hasEntryRecursive(TiffTag tag) const { return getEntryRecursive(tag) != nullptr; }

  const vector<TiffIFDOwner>& getSubIFDs() const { return subIFDs; }
//  const map<TiffTag, TiffEntry*>& getEntries() const { return entries; }
};

class TiffRootIFD : public TiffIFD
{
public:
  const DataBuffer rootBuffer;

  TiffRootIFD(DataBuffer data, uint32 offset) : TiffIFD(data, offset, nullptr), rootBuffer(data) {}
};

inline bool isTiffInNativeByteOrder(const ByteStream& bs, uint32 pos, const char* context = "") {
  if (bs.hasPatternAt("II", 2, pos))
    return getHostEndianness() == little;
  else if (bs.hasPatternAt("MM", 2, pos))
    return getHostEndianness() == big;
  else
    ThrowTPE("Failed to parse TIFF endianess information in %s.", context);
  return true; // prevent compiler warning
}

inline Endianness getTiffEndianness(const FileMap* file) {
  ushort16 magic = *(ushort16*)file->getData(0, 2);
  if (magic == 0x4949)
    return little;
  else if (magic == 0x4d4d)
    return big;
  else
    ThrowTPE("Failed to parse TIFF endianess information.");
  return unknown; // prevent compiler warning
}

} // namespace RawSpeed

#endif
