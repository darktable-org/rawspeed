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
*/

#pragma once

#include "common/Common.h"               // for uint32, ushort16
#include "io/Buffer.h"                   // for Buffer (ptr only), DataBuffer
#include "io/ByteStream.h"               // for ByteStream
#include "io/Endianness.h"               // for getHostEndianness, Endianne...
#include "io/FileMap.h"                  // for FileMap
#include "parsers/TiffParserException.h" // for ThrowTPE
#include "tiff/TiffTag.h"                // for TiffTag
#include <map>                           // for map, _Rb_tree_const_iterator
#include <memory>                        // for unique_ptr
#include <string>                        // for string
#include <vector>                        // for vector

namespace RawSpeed {

class TiffEntry;

class TiffIFD;

class TiffRootIFD;

using TiffIFDOwner = std::unique_ptr<TiffIFD>;
using TiffRootIFDOwner = std::unique_ptr<TiffRootIFD>;
using TiffEntryOwner = std::unique_ptr<TiffEntry>;

class TiffIFD
{
  uint32 nextIFD = 0;
  TiffIFD* parent = nullptr;
  std::vector<TiffIFDOwner> subIFDs;
  std::map<TiffTag, TiffEntryOwner> entries;

  friend class TiffEntry;
  friend class FiffParser;
  friend TiffRootIFDOwner parseTiff(const Buffer& data);

  // make sure we never copy-constuct/assign a TiffIFD to keep the owning subcontainers contents save
  TiffIFD(const TiffIFD &) = delete;            // NOLINT
  TiffIFD &operator=(const TiffIFD &) = delete; // NOLINT

  void add(TiffIFDOwner subIFD);
  void add(TiffEntryOwner entry);
  TiffRootIFDOwner parseDngPrivateData(TiffEntry *t);
  TiffRootIFDOwner parseMakerNote(TiffEntry *t);
  void parseIFDEntry(ByteStream& bs);

public:
  TiffIFD() = default;
  TiffIFD(const DataBuffer& data, uint32 offset, TiffIFD* parent);
  virtual ~TiffIFD() = default;
  uint32 getNextIFD() const {return nextIFD;}
  std::vector<const TiffIFD*> getIFDsWithTag(TiffTag tag) const;
  const TiffIFD* getIFDWithTag(TiffTag tag, uint32 index = 0) const;
  TiffEntry* getEntry(TiffTag tag) const;
  TiffEntry* __attribute__((pure)) getEntryRecursive(TiffTag tag) const;
  bool __attribute__((pure)) hasEntry(TiffTag tag) const {
    return entries.find(tag) != entries.end();
  }
  bool hasEntryRecursive(TiffTag tag) const { return getEntryRecursive(tag) != nullptr; }

  const std::vector<TiffIFDOwner>& getSubIFDs() const { return subIFDs; }
//  const std::map<TiffTag, TiffEntry*>& getEntries() const { return entries; }
};

struct TiffID
{
  std::string make;
  std::string model;
};

class TiffRootIFD final : public TiffIFD {
public:
  const DataBuffer rootBuffer;

  TiffRootIFD(const DataBuffer &data, uint32 offset)
      : TiffIFD(data, offset, nullptr), rootBuffer(data) {}

  // find the MAKE and MODEL tags identifying the camera
  // note: the returned strings are trimmed automatically
  TiffID getID() const;
};

inline bool isTiffInNativeByteOrder(const ByteStream& bs, uint32 pos, const char* context = "") {
  if (bs.hasPatternAt("II", 2, pos))
    return getHostEndianness() == little;
  if (bs.hasPatternAt("MM", 2, pos))
    return getHostEndianness() == big;

  ThrowTPE("Failed to parse TIFF endianess information in %s.", context);
}

inline Endianness getTiffEndianness(const FileMap* file) {
  ushort16 magic = *(ushort16*)file->getData(0, 2);
  if (magic == 0x4949)
    return little;
  if (magic == 0x4d4d)
    return big;

  ThrowTPE("Failed to parse TIFF endianess information.");
}

} // namespace RawSpeed
