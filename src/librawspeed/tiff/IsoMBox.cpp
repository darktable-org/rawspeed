/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2018 Roman Lebedev
    Copyright (C) 2021 Daniel Vogelbacher

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

#include "tiff/IsoMBox.h"
#include "common/NORangesSet.h"          // for NORangesSet
#include "io/Buffer.h"                   // for Buffer::size_type
#include "parsers/IsoMParserException.h" // for ThrowIPE, IsoMParserException
#include <algorithm>                     // for find, generate_n
#include <cassert>                       // for assert
#include <cstring>                       // for memcmp
#include <limits>                        // for numeric_limits
#include <utility>                       // for pair

namespace rawspeed {


// The ODR-definitions

const FourCharStr IsoMBoxTypes::ftyp;
const FourCharStr IsoMBoxTypes::co64;
const FourCharStr IsoMBoxTypes::stsz;
const FourCharStr IsoMBoxTypes::stsc;
const FourCharStr IsoMBoxTypes::stsd;
const FourCharStr IsoMBoxTypes::stbl;
const FourCharStr IsoMBoxTypes::url;
const FourCharStr IsoMBoxTypes::dref;
const FourCharStr IsoMBoxTypes::dinf;
const FourCharStr IsoMBoxTypes::minf;
const FourCharStr IsoMBoxTypes::mdia;
const FourCharStr IsoMBoxTypes::trak;
const FourCharStr IsoMBoxTypes::moov;
const FourCharStr IsoMBoxTypes::mdat;

const FourCharStr IsoMBoxTypes::uuid;


// Base-level lexing/parsing.

AbstractIsoMBox::AbstractIsoMBox(ByteStream* bs) {
  const auto origPos = bs->getPosition();

  // This is the size of this whole box, starting from the origPos.
  const auto boxSize = bs->getU32();

  boxType = FourCharStr(bs->getU32());

  if (boxSize == 0) {
    bs->setPosition(origPos);
    // Rest is the whole box.
    data = bs->getStream(bs->getRemainSize());
  } else if (boxSize != 1) {
    bs->setPosition(origPos);
    assert(bs->getRemainSize() >= boxSize);
    // The good case, this is the size of the box.
    data = bs->getStream(boxSize);
  } else {
    // Meh, the ugly case :/
    assert(boxSize == 1);
    const auto largeSize = bs->get<uint64_t>();

    // The rawspeed::Buffer is 32-bit, so even we somehow get here with valid
    // more-than 32-bit-sized box, we can't do anything about it.
    // We have to handle this explicitly because else 64-bit will get truncated
    // to 32-bit without us noticing, and nothing good will happen next.
    if (largeSize > std::numeric_limits<Buffer::size_type>::max())
      ThrowIPE("IsoM Box uses largesize which does not fit into 32-bits");

    bs->setPosition(origPos);
    assert(bs->getRemainSize() >= largeSize);
    data = bs->getStream(static_cast<Buffer::size_type>(largeSize));
    data.skipBytes(8); // skip the largeSize,
  }

  data.skipBytes(8); // already read those before in any case

  if (FourCharStr({'u', 'u', 'i', 'd'}) == boxType) {
    const auto userTypeBs = data.getBuffer(16);
    std::copy(userTypeBs.begin(), userTypeBs.end(), userType.begin());
  }
}

void IsoMContainer::lexBox() { boxes.emplace_back(&cData); }

void IsoMContainer::lexSubBoxes() {
  // A box is a series of boxes.
  while (cData.getRemainSize() > 0)
    lexBox();
  // There is nothing else left after boxes.
  assert(cData.getRemainSize() == 0);
}

IsoMContainer::IsoMContainer(ByteStream* bs)
    : cData(bs->getStream(bs->getRemainSize())) {
  lexSubBoxes();
  // There is nothing else left after boxes.
  assert(cData.getRemainSize() == 0);
}

const AbstractIsoMBox&
IsoMContainer::getBox(const AbstractIsoMBox::UuidType& uuid) const {
  for(const auto& box : boxes) {
    if(uuid == box.userType) {
      return box;
    }
  }
  ThrowIPE("Requested box UUID not found");
}



// FileType box parsing.

const std::array<const FourCharStr, 1> IsoMFileTypeBox::supportedBrands;
IsoMFileTypeBox::operator bool() const {
  if (std::find(supportedBrands.begin(), supportedBrands.end(), majorBrand) ==
      supportedBrands.end())
    ThrowIPE("Unsupported major brand: %s", majorBrand.str().c_str());

  bool isComp = false;
  for (const auto& compatibleBrand : compatibleBrands) {
    isComp = std::find(supportedBrands.begin(), supportedBrands.end(),
                       compatibleBrand) != supportedBrands.end();
    if (isComp)
      break;
  }
  if (!isComp)
    ThrowIPE("No intersection between compatibleBrands and supported brands");

  return true; // Supported!
}

IsoMFileTypeBox::IsoMFileTypeBox(const AbstractIsoMBox& base) : BaseBox(base) {
  majorBrand = FourCharStr(data.getU32());
  minorVersion = data.getU32();
  while (data.getRemainSize() > 0)
    compatibleBrands.emplace_back(data.getU32());
  // There is nothing else left.
  assert(data.getRemainSize() == 0);

  // Validate.
  operator bool();
}

// SampleDescription box parsing.

IsoMSampleDescriptionBox::SampleEntry::SampleEntry(ByteStream* bs)
    : AbstractIsoMBox(bs) {
  for (auto& c : reserved)
    c = data.getByte();
  dataReferenceIndex = data.getU16();
}

IsoMSampleDescriptionBox::operator bool() const {
  if (dscs.size() != 1)
    ThrowIPE("Unexpected entry count: %zu", dscs.size());

  for (const auto& dsc : dscs) {
    if (dsc.dataReferenceIndex != 1)
      ThrowIPE("Unexpected data reference index: %u", dsc.dataReferenceIndex);
  }

  return true; // Supported!
}

IsoMSampleDescriptionBox::IsoMSampleDescriptionBox(const AbstractIsoMBox& base)
    : IsoMFullBox(base) {
  const auto entryCount = data.getU32();

  // Can't check/reserve entryCount.
  std::generate_n(std::back_inserter(dscs), entryCount,
                  [this]() { return SampleEntry(&data); });
  assert(dscs.size() == entryCount);

  // Validate.
  operator bool();
}



IsoMSampleToChunkBox::operator bool() const {
  if (dscs.size() != 1)
    ThrowIPE("Unexpected entry count: %zu", dscs.size());

  for (const auto& dsc : dscs) {
    if (dsc.firstChunk != 1)
      ThrowIPE("Unexpected first chunk: %u", dsc.firstChunk);
    if (dsc.samplesPerChunk != 1)
      ThrowIPE("Unexpected samples per chunk: %u", dsc.samplesPerChunk);
    if (dsc.sampleDescriptionIndex != 1) {
      ThrowIPE("Unexpected sample description index: %u",
               dsc.sampleDescriptionIndex);
    }
  }

  return true; // Supported!
}

IsoMSampleToChunkBox::IsoMSampleToChunkBox(const AbstractIsoMBox& base)
    : IsoMFullBox(base) {
  const auto entryCount = data.getU32();

  data.check(entryCount, 3 * 4);
  dscs.reserve(entryCount);
  std::generate_n(std::back_inserter(dscs), entryCount, [this]() {
    Dsc d;
    d.firstChunk = data.getU32();
    d.samplesPerChunk = data.getU32();
    d.sampleDescriptionIndex = data.getU32();
    return d;
  });
  assert(dscs.size() == entryCount);

  // Validate.
  operator bool();
}

// SampleSize box parsing.

IsoMSampleSizeBox::operator bool() const {
  if (chunkSizes.empty())
    ThrowIPE("No chunk sizes found");

  // The actual validation of these values will happen
  // during parsing of moov box.

  return true; // Supported!
}

IsoMSampleSizeBox::IsoMSampleSizeBox(const AbstractIsoMBox& base)
    : IsoMFullBox(base) {
  const auto sampleSize = data.getU32();
  const auto sampleCount = data.getU32();

  if (sampleSize == 0) {
    for(uint32_t i = 0; i < sampleCount; ++i) {
      chunkSizes.emplace_back(data.getU32());
    }
  } else {
    // It's the only sample size and it is stored
    // in the sampleSize directly.
    chunkSizes.emplace_back(sampleSize);
  }

  // Validate.
  operator bool();
}

// ChunkLargeOffset box parsing.

IsoMChunkLargeOffsetBox::operator bool() const {
  if (chunkOffsets.empty())
    ThrowIPE("No chunk offsets found");

  // The actual validation of these values will happen
  // during parsing of moov box.

  return true; // Supported!
}

IsoMChunkLargeOffsetBox::IsoMChunkLargeOffsetBox(const AbstractIsoMBox& base)
    : IsoMFullBox(base) {
  const auto entryCount = data.getU32();
  data.check(entryCount, 8);

  if (entryCount != 1)
    ThrowIPE("Don't know how to handle co64 box with %u entries", entryCount);

  chunkOffsets.reserve(entryCount);
  std::generate_n(
      std::back_inserter(chunkOffsets), entryCount,
      [this]() -> Buffer::size_type {
        const auto largeSize = data.get<uint64_t>();

        // The rawspeed::Buffer is 32-bit, so even we somehow get here with
        // valid more-than 32-bit-sized box, we can't do anything about it. We
        // have to handle this explicitly because else 64-bit will get truncated
        // to 32-bit without us noticing, and nothing good will happen next.
        if (largeSize > std::numeric_limits<Buffer::size_type>::max())
          ThrowIPE("IsoM Box uses largesize which does not fit into 32-bits");
        return static_cast<Buffer::size_type>(largeSize);
      });
  assert(chunkOffsets.size() == entryCount);
  // Could still have some padding bytes left, but don't care.

  // Validate.
  operator bool();
}

// Sample Table box handling.

void IsoMSampleTableBox::parseBox(const AbstractIsoMBox& box) {
  if (IsoMSampleDescriptionBox::BoxType == box.boxType) {
    if (stsd)
      ThrowIPE("duplicate stsd box found.");
    stsd = AbstractIsoMBox::ParseBox<IsoMSampleDescriptionBox>(box);
    return;
  }
  if (IsoMSampleToChunkBox::BoxType == box.boxType) {
    if (stsc)
      ThrowIPE("duplicate stsc box found.");
    stsc = AbstractIsoMBox::ParseBox<IsoMSampleToChunkBox>(box);
    return;
  }
  if (IsoMSampleSizeBox::BoxType == box.boxType) {
    if (stsz)
      ThrowIPE("duplicate stsz box found.");
    stsz = AbstractIsoMBox::ParseBox<IsoMSampleSizeBox>(box);
    return;
  }
  if (IsoMChunkLargeOffsetBox::BoxType == box.boxType) {
    if (co64)
      ThrowIPE("duplicate co64 box found.");
    co64 = AbstractIsoMBox::ParseBox<IsoMChunkLargeOffsetBox>(box);
    return;
  }
}

IsoMSampleTableBox::operator bool() const {
  if (!stsd)
    ThrowIPE("no stsd box found.");
  if (!stsc)
    ThrowIPE("no stsc box found.");
  if (!stsz)
    ThrowIPE("no stsz box found.");
  if (!co64)
    ThrowIPE("no co64 box found.");

  if (stsz->chunkSizes.size() != co64->chunkOffsets.size())
    ThrowIPE("Mismatch in chunk offset and size count.");
  if (stsc->dscs.size() != co64->chunkOffsets.size())
    ThrowIPE("Mismatch in stsc entry count and chunk offset count.");
  if (stsc->dscs.size() != stsd->dscs.size())
    ThrowIPE("Mismatch in stsc entry count and stsd entry count.");

  return true; // OK!
}

// DataReference box parsing.

IsoMDataReferenceBox::IsoMDataEntryUrlBox::operator bool() const {
  if (flags != static_cast<decltype(flags)>(Flags::SelfContained))
    ThrowIPE("Unexpected flags: %u; entry is not self-contained", flags);

  return true; // Supported!
}

IsoMDataReferenceBox::IsoMDataEntryUrlBox::IsoMDataEntryUrlBox(
    const AbstractIsoMBox& base)
    : IsoMFullBox(base) {
  // Validate.
  operator bool();
}

IsoMDataReferenceBox::operator bool() const {
  if (entries.size() != 1)
    ThrowIPE("Unexpected entry count: %zu", entries.size());

  return true; // Supported!
}

IsoMDataReferenceBox::IsoMDataReferenceBox(const AbstractIsoMBox& base)
    : IsoMFullBox(base) {
  const auto entryCount = data.getU32();

  for (auto entry = 1U; entry <= entryCount; entry++) {
    auto box = AbstractIsoMBox(&data);
    if (IsoMDataEntryUrlBox::BoxType == box.boxType) {
      entries.emplace_back(box);
      entries.back().parse();
    }
  }

  // Validate.
  operator bool();
}

// Data Information box handling.

void IsoMDataInformationBox::parseBox(const AbstractIsoMBox& box) {
  if (IsoMDataReferenceBox::BoxType == box.boxType) {
    if (dref)
      ThrowIPE("duplicate dref box found.");
    dref = AbstractIsoMBox::ParseBox<IsoMDataReferenceBox>(box);
    return;
  }
}

IsoMDataInformationBox::operator bool() const {
  if (!dref)
    ThrowIPE("no dref box found.");

  return true; // OK!
}

// Media Information box handling.

void IsoMMediaInformationBox::parseBox(const AbstractIsoMBox& box) {
  if (IsoMDataInformationBox::BoxType == box.boxType) {
    if (dinf)
      ThrowIPE("duplicate dinf box found.");
    dinf = AbstractIsoMBox::ParseBox<IsoMDataInformationBox>(box);
    return;
  }
  if (IsoMSampleTableBox::BoxType == box.boxType) {
    if (stbl)
      ThrowIPE("duplicate stbl box found.");
    stbl = AbstractIsoMBox::ParseBox<IsoMSampleTableBox>(box);
    return;
  }
}

IsoMMediaInformationBox::operator bool() const {
  if (!dinf)
    ThrowIPE("no dinf box found.");
  if (!stbl)
    ThrowIPE("no stbl box found.");

  if (dinf->dref->entries.size() != stbl->stsd->dscs.size())
    ThrowIPE("Mismatch in dref entry count and stsd entry count.");

  return true; // OK!
}

// Media box handling.

void IsoMMediaBox::parseBox(const AbstractIsoMBox& box) {
  if (IsoMMediaInformationBox::BoxType == box.boxType) {
    if (minf)
      ThrowIPE("duplicate minf box found.");
    minf = AbstractIsoMBox::ParseBox<IsoMMediaInformationBox>(box);
    return;
  }
}

IsoMMediaBox::operator bool() const {
  if (!minf)
    ThrowIPE("no minf box found.");

  return true; // OK!
}

// Track box handling.

void IsoMTrackBox::parseBox(const AbstractIsoMBox& box) {
  if (IsoMMediaBox::BoxType == box.boxType) {
    if (mdia)
      ThrowIPE("duplicate mdia box found.");
    mdia = AbstractIsoMBox::ParseBox<IsoMMediaBox>(box);
    return;
  }
}

IsoMTrackBox::operator bool() const {
  if (!mdia)
    ThrowIPE("no mdia box found.");

  return true; // OK!
}

// Movie box handling.

void IsoMMovieBox::parseBox(const AbstractIsoMBox& box) {
  if (IsoMTrackBox::BoxType == box.boxType) {
    tracks.emplace_back(box);
    tracks.back().parse();
    return;
  }
}

IsoMMovieBox::operator bool() const {
  if (tracks.empty())
    ThrowIPE("no track boxes found.");


  return true; // OK!
}

// Media Data box handling.

IsoMMediaDataBox::operator bool() const {
  if (chunks.empty())
    ThrowIPE("no chunks found.");

  return true; // OK!
}

void IsoMMediaDataBox::parse(IsoMRootBox* root) {
  assert(root);

  // Visit each sample (offset+size pair) in each track.
  auto forEachChunk = [root](auto fun) {
    for (const auto& track : root->moov()->tracks) {
      auto& stbl = track.mdia->minf->stbl;
      const auto& stsz = stbl->stsz;
      const auto& co64 = stbl->co64;
      assert(stsz->chunkSizes.size() == co64->chunkOffsets.size());
      const auto numChunks = stsz->chunkSizes.size();
      stbl->chunks.reserve(numChunks);
      for (auto chunk = 0U; chunk < numChunks; chunk++) {
        fun(co64->chunkOffsets[chunk], stsz->chunkSizes[chunk],
            std::back_inserter(stbl->chunks));
      }
    }
  };

  const unsigned numChunks = [&forEachChunk]() -> unsigned {
    unsigned i = 0;
    // Just count them all.
    forEachChunk(
        [&i](Buffer::size_type, Buffer::size_type,
             std::back_insert_iterator<std::vector<const ByteStream*>>) {
          i++;
        });
    return i;
  }();
  chunks.reserve(numChunks);

  // chunk legality checks
  NORangesSet<Buffer> clc;

  forEachChunk(
      [root, &clc, this](
          Buffer::size_type offset, Buffer::size_type count,
          std::back_insert_iterator<std::vector<const ByteStream*>> stblChunk) {
        // The offset is global to the file (eww, ugh!).
        const auto chunk = root->cData.getSubStream(offset, count);
        // Is it actually in the mdat box?
        if (!RangesAreNested(mData, chunk))
          ThrowIPE("Chunk is not in the mdat box.");
        // Does it overlap with any previous chunk?
        if (!clc.emplace(chunk).second)
          ThrowIPE("Two chunks overlap.");
        // OK!
        chunks.emplace_back(chunk);
        stblChunk = &(chunks.back());
      });
  assert(chunks.size() == numChunks);

  // Validate.
  operator bool();
}

// The handling of the root container.

void IsoMRootBox::parseBox(const AbstractIsoMBox& box) {
  if (IsoMFileTypeBox::BoxType == box.boxType) {
    if (ftypBox)
      ThrowIPE("duplicate ftyp box found.");
    ftypBox = AbstractIsoMBox::ParseBox<IsoMFileTypeBox>(box);
    return;
  }
  if (IsoMMovieBox::BoxType == box.boxType) {
    if (!ftypBox)
      ThrowIPE("no ftyp box found yet.");
    if (moovBox)
      ThrowIPE("duplicate moov box found.");
    moovBox = AbstractIsoMBox::ParseBox<IsoMMovieBox>(box);
    return;
  }
  if (IsoMMediaDataBox::BoxType == box.boxType) {
    if (!moovBox)
      ThrowIPE("no moov box found yet.");
    if (mdatBox)
      ThrowIPE("duplicate mdat box found.");
    mdatBox = AbstractIsoMBox::ParseBox<IsoMMediaDataBox>(box, this);
    return;
  }
}

IsoMRootBox::operator bool() const {
  if (!ftypBox)
    ThrowIPE("ftyp box not found.");
  if (!moovBox)
    ThrowIPE("moov box not found.");
  if (!mdatBox)
    ThrowIPE("mdat box not found.");

  return true; // OK!
}

const std::unique_ptr<IsoMFileTypeBox>& IsoMRootBox::ftyp() const {
  if(ftypBox)
    return ftypBox;
  else
    ThrowIPE("ftyp box not available");
}
const std::unique_ptr<IsoMMovieBox>& IsoMRootBox::moov() const {
  if(moovBox)
    return moovBox;
  else
    ThrowIPE("moov box not available");
}
const std::unique_ptr<IsoMMediaDataBox>& IsoMRootBox::mdat() const {
  if(mdatBox)
    return mdatBox;
  else
    ThrowIPE("mdat box not available");
}


} // namespace rawspeed
