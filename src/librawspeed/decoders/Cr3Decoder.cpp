/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2018 Roman Lebedev
    Copyright (C) 2021 Daniel Vogelbacher

    Information about CR3 file structure and BMFF boxes
    provided by Laurent Clévy and contributors
    via https://github.com/lclevy/canon_cr3


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

#include "decoders/Cr3Decoder.h"           // for Cr3Decoder
#include "decompressors/CrxDecompressor.h" // for CrxDecompressor
#include "parsers/IsoMParserException.h"   // for ThrowIPE
#include "parsers/TiffParser.h"            // for TiffParser

namespace rawspeed {

const FourCharStr IsoMBoxCanonTypes::CNCV;
const FourCharStr IsoMBoxCanonTypes::CCTP;
const FourCharStr IsoMBoxCanonTypes::CTBO;
const FourCharStr IsoMBoxCanonTypes::CMT1;
const FourCharStr IsoMBoxCanonTypes::CMT2;
const FourCharStr IsoMBoxCanonTypes::CMT3;
const FourCharStr IsoMBoxCanonTypes::CMT4;
const FourCharStr IsoMBoxCanonTypes::THMB;
const FourCharStr IsoMBoxCanonTypes::CRAW;
const FourCharStr IsoMBoxCanonTypes::CMP1;
const FourCharStr IsoMBoxCanonTypes::CDI1;
const FourCharStr IsoMBoxCanonTypes::IAD1;
const FourCharStr IsoMBoxCanonTypes::CTMD;

const AbstractIsoMBox::UuidType CanonBoxUuid = {
    0x85, 0xc0, 0xb6, 0x87, 0x82, 0x0f, 0x11, 0xe0,
    0x81, 0x11, 0xf4, 0xce, 0x46, 0x2b, 0x6a, 0x48};

void IsoMCanonBox::parseBox(const AbstractIsoMBox& box) {
  if (IsoMCanonCodecVersionBox::BoxType == box.boxType) {
    if (cncvBox)
      ThrowIPE("duplicate cncv box found.");
    cncvBox = AbstractIsoMBox::ParseBox<IsoMCanonCodecVersionBox>(box);
    return;
  }
  if (IsoMCanonCCTPBox::BoxType == box.boxType) {
    if (cctpBox)
      ThrowIPE("duplicate CCTP box found.");
    cctpBox = AbstractIsoMBox::ParseBox<IsoMCanonCCTPBox>(box);
    return;
  }
  if (IsoMCanonCTBOBox::BoxType == box.boxType) {
    if (ctboBox)
      ThrowIPE("duplicate CTBO box found.");
    ctboBox = AbstractIsoMBox::ParseBox<IsoMCanonCTBOBox>(box);
    return;
  }
  if (IsoMCanonCMT1Box::BoxType == box.boxType) {
    if (cmt1Box)
      ThrowIPE("duplicate CMT1 box found.");
    cmt1Box = AbstractIsoMBox::ParseBox<IsoMCanonCMT1Box>(box);
    return;
  }
  if (IsoMCanonCMT2Box::BoxType == box.boxType) {
    if (cmt2Box)
      ThrowIPE("duplicate CMT2 box found.");
    cmt2Box = AbstractIsoMBox::ParseBox<IsoMCanonCMT2Box>(box);
    return;
  }
  if (IsoMCanonCMT3Box::BoxType == box.boxType) {
    if (cmt3Box)
      ThrowIPE("duplicate CMT3 box found.");
    cmt3Box = AbstractIsoMBox::ParseBox<IsoMCanonCMT3Box>(box);
    return;
  }
  if (IsoMCanonCMT4Box::BoxType == box.boxType) {
    if (cmt4Box)
      ThrowIPE("duplicate CMT4 box found.");
    cmt4Box = AbstractIsoMBox::ParseBox<IsoMCanonCMT4Box>(box);
    return;
  }

  if (IsoMCanonThumbnailBox::BoxType == box.boxType) {
    if (thmbBox)
      ThrowIPE("duplicate THMB box found.");
    thmbBox = AbstractIsoMBox::ParseBox<IsoMCanonThumbnailBox>(box);
    return;
  }
}

const std::unique_ptr<IsoMCanonCodecVersionBox>& IsoMCanonBox::CNCV() const {
  if (cncvBox)
    return cncvBox;
  else
    ThrowIPE("CNCV box not available");
}

const std::unique_ptr<IsoMCanonCCTPBox>& IsoMCanonBox::CCTP() const {
  if (cctpBox)
    return cctpBox;
  else
    ThrowIPE("CCTP box not available");
}

const std::unique_ptr<IsoMCanonCTBOBox>& IsoMCanonBox::CTBO() const {
  if (ctboBox)
    return ctboBox;
  else
    ThrowIPE("CTBO box not available");
}

const std::unique_ptr<IsoMCanonCMT1Box>& IsoMCanonBox::CMT1() const {
  if (cmt1Box)
    return cmt1Box;
  else
    ThrowIPE("CMT1 box not available");
}

const std::unique_ptr<IsoMCanonCMT2Box>& IsoMCanonBox::CMT2() const {
  if (cmt2Box)
    return cmt2Box;
  else
    ThrowIPE("CMT2 box not available");
}

const std::unique_ptr<IsoMCanonCMT3Box>& IsoMCanonBox::CMT3() const {
  if (cmt3Box)
    return cmt3Box;
  else
    ThrowIPE("CMT3 box not available");
}

const std::unique_ptr<IsoMCanonCMT4Box>& IsoMCanonBox::CMT4() const {
  if (cmt4Box)
    return cmt4Box;
  else
    ThrowIPE("CMT4 box not available");
}

const std::unique_ptr<IsoMCanonThumbnailBox>& IsoMCanonBox::THMB() const {
  if (thmbBox)
    return thmbBox;
  else
    ThrowIPE("THMB box not available");
}

IsoMCanonBox::operator bool() const {
  if (!cncvBox)
    ThrowIPE("no CNCV box found.");
  if (!cctpBox)
    ThrowIPE("no CCTP box found.");
  if (!ctboBox)
    ThrowIPE("no CTBO box found.");
  if (!cmt1Box)
    ThrowIPE("no CMT1 box found.");
  if (!cmt2Box)
    ThrowIPE("no CMT2 box found.");
  if (!cmt3Box)
    ThrowIPE("no CMT3 box found.");
  if (!cmt4Box)
    ThrowIPE("no CMT4 box found.");

  return true; // OK!
}

IsoMCanonCodecVersionBox::IsoMCanonCodecVersionBox(const AbstractIsoMBox& base)
    : IsoMBox(base) {
  assert(data.getRemainSize() == 30); // Payload string is exactly 30 bytes long
  auto payload = data.getBuffer(30);
  compressorVersion = std::string(payload.begin(), payload.end());
  assert(data.getRemainSize() == 0);
}

IsoMCanonCMT1Box::IsoMCanonCMT1Box(const AbstractIsoMBox& base)
    : IsoMBox(base) {
  NORangesSet<Buffer> rs;
  auto payload =
      DataBuffer(data.getBuffer(data.getRemainSize()), Endianness::little);
  mRootIFD0 = TiffParser::parse(nullptr, payload);
}

IsoMCanonCMT2Box::IsoMCanonCMT2Box(const AbstractIsoMBox& base)
    : IsoMBox(base) {
  NORangesSet<Buffer> rs;
  auto payload =
      DataBuffer(data.getBuffer(data.getRemainSize()), Endianness::little);
  mRootIFD0 = TiffParser::parse(nullptr, payload);
}

IsoMCanonCMT3Box::IsoMCanonCMT3Box(const AbstractIsoMBox& base)
    : IsoMBox(base) {
  NORangesSet<Buffer> rs;
  auto payload =
      DataBuffer(data.getBuffer(data.getRemainSize()), Endianness::little);
  mRootIFD0 = TiffParser::parse(nullptr, payload);
}

IsoMCanonCMT4Box::IsoMCanonCMT4Box(const AbstractIsoMBox& base)
    : IsoMBox(base) {
  NORangesSet<Buffer> rs;
  auto payload =
      DataBuffer(data.getBuffer(data.getRemainSize()), Endianness::little);
  mRootIFD0 = TiffParser::parse(nullptr, payload);
}

IsoMCanonTimedMetadataBox::IsoMCanonTimedMetadataBox(
    const AbstractIsoMBox& base)
    : IsoMBox(base) {
  // Set position after box `size` and `boxtype` fields, so we
  // can parse the custom SampleEntry ourself.
  data.setPosition(8);

  for (auto& c : reserved1)
    c = data.getByte();
  dataReferenceIndex = data.getU16();

  const auto entryCount = data.getU32();

  // Can't check/reserve entryCount.
  std::generate_n(std::back_inserter(recDescs), entryCount,
                  [this]() { return RecordDesc(&data); });
  assert(recDescs.size() == entryCount);

  assert(data.getRemainSize() == 0);

  // Validate.
  operator bool();
}

IsoMCanonTimedMetadataBox::operator bool() const {
  // This CTMD box is not used for decoding, since record type and size
  // are available in MDAT data for CTMD, too.
  return true; // OK!
}

IsoMCanonTimedMetadataBox::RecordDesc::RecordDesc(ByteStream* bs) {
  recType = bs->getU32();
  recSize = bs->getU32();
}

IsoMCanonCrawBox::IsoMCanonCrawBox(const AbstractIsoMBox& base)
    : IsoMBox(base) {
  // Set position after box `size` and `boxtype` fields, so we
  // can parse the custom SampleEntry ourself.
  data.setPosition(8);

  for (auto& c : reserved1)
    c = data.getByte();
  dataReferenceIndex = data.getU16();
  for (auto& c : reserved2)
    c = data.getByte();
  width = data.getU16();
  height = data.getU16();
  xResolution = static_cast<uint32_t>(data.getU16()) << 16 | data.getU16();
  yResolution = static_cast<uint32_t>(data.getU16()) << 16 | data.getU16();
  reserved3 = data.getU32();
  reserved4 = data.getU16();
  for (auto& c : reserved5)
    c = data.getByte();
  bitDepth = data.getU16();
  reserved6 = data.getU16();
  flags = data.getU16();
  formatInd = data.getU16();

  // Change this if Canon adds more fields to CRAW box
  assert(data.getPosition() == 90);

  // After fields, there are embedded boxes
  cmp1Box = std::make_unique<IsoMCanonCmp1Box>(AbstractIsoMBox(&data));
  cdi1Box = std::make_unique<IsoMCanonCdi1Box>(AbstractIsoMBox(&data));
  cdi1Box->IsoMContainer::parse();
  // There is a 'free' box after CDI1 which we ignore

  // Validate.
  operator bool();
}

IsoMCanonCrawBox::operator bool() const {
  // For JPEG trak, CRAW has no CMP1/CDI1 boxes. But as we
  // decode RAW, not JPEG, CMP1 and CDI1 are required.
  if (!cmp1Box)
    ThrowIPE("no CMP1 box found.");
  if (!cdi1Box)
    ThrowIPE("no CDI1 box found.");

  return true; // OK!
}

const std::unique_ptr<IsoMCanonCmp1Box>& IsoMCanonCrawBox::CMP1() const {
  if (cmp1Box)
    return cmp1Box;
  else
    ThrowIPE("CMP1 box not available");
}

const std::unique_ptr<IsoMCanonCdi1Box>& IsoMCanonCrawBox::CDI1() const {
  if (cdi1Box)
    return cdi1Box;
  else
    ThrowIPE("CDI1 box not available");
}

IsoMCanonCmp1Box::IsoMCanonCmp1Box(const AbstractIsoMBox& base)
    : IsoMBox(base) {
  // Set position after box `size` and `boxtype` fields, so we
  // can parse the custom SampleEntry ourself.
  data.setPosition(8);
  // This fields mainly used in the decoding process.
  reserved1 = data.getU16();
  headerSize = data.getU16();
  assert(headerSize == 0x30);
  version = data.getI16();
  versionSub = data.getI16();
  f_width = data.getI32();
  f_height = data.getI32();
  tileWidth = data.getI32();
  tileHeight = data.getI32();
  nBits = data.get<int8_t>();
  nPlanes = data.peek<int8_t>() >> 4;
  cfaLayout = data.get<int8_t>() & 0xF;
  encType = data.peek<int8_t>() >> 4;
  imageLevels = data.get<int8_t>() & 0xF;
  hasTileCols = data.peek<int8_t>() >> 7;
  hasTileRows = data.get<int8_t>() & 1;
  mdatHdrSize = data.getI32();
  // Some reserved fields, unknown.
  reserved2 = data.getI32();
  for (auto& c : reserved3)
    c = data.getByte();

  // we assume this is fixed, until Canon makes CMP1 flexible
  assert(data.getPosition() == 44 + 16);
  // headerSize should match position
  assert((data.getPosition() - 2 - 2 - 8) == headerSize);
  assert(data.getRemainSize() == 0);

  // Validate.
  operator bool();
}

IsoMCanonCmp1Box::operator bool() const {
  // validation based on libraw decoder requirements
  if (version != 0x100 && version != 0x200) {
    ThrowRDE("Unsupported version in CMP1");
  }
  if (!mdatHdrSize) {
    ThrowRDE("CMP1 describes an empty MDAT header");
  }
  if (encType == 1) {
    if (nBits > 15)
      ThrowRDE("Unknown encoding bit count in CMP1");
  } else {
    if (encType && encType != 3)
      ThrowRDE("Unknown encType in CMP1");
    if (nBits > 14)
      ThrowRDE("Unknown encoding bit count in CMP1");
  }
  if (nPlanes == 1) {
    if (cfaLayout || encType || nBits != 8)
      ThrowRDE("Unknown encoding parameters in CMP1");
  } else if (nPlanes != 4 || f_width & 1 || f_height & 1 || tileWidth & 1 ||
             tileHeight & 1 || cfaLayout > 3 || nBits == 8)
    ThrowRDE("Unknown encoding parameters in CMP1");

  if (tileWidth > f_width || tileHeight > f_height)
    ThrowRDE("Unknown encoding parameters in CMP1");

  if (imageLevels > 3 || hasTileCols > 1 || hasTileRows > 1)
    ThrowRDE("Unknown encoding parameters in CMP1");

  return true; // OK!
}

void IsoMCanonCdi1Box::parseBox(const AbstractIsoMBox& box) {
  if (IsoMCanonIad1Box::BoxType == box.boxType) {
    if (iad1Box)
      ThrowIPE("duplicate IAD1 box found.");
    iad1Box = AbstractIsoMBox::ParseBox<IsoMCanonIad1Box>(box);
    return;
  }
}

IsoMCanonCdi1Box::operator bool() const {
  if (!iad1Box)
    ThrowIPE("no IAD1 box found.");

  return true; // OK!
}

const std::unique_ptr<IsoMCanonIad1Box>& IsoMCanonCdi1Box::IAD1() const {
  if (iad1Box)
    return iad1Box;
  else
    ThrowIPE("IAD1 box not available");
}

IsoMCanonIad1Box::IsoMCanonIad1Box(const AbstractIsoMBox& base)
    : IsoMFullBox(base) {
  sensorWidth = data.get<uint16_t>();
  sensorHeight = data.get<uint16_t>();
  reserved1 = data.get<uint16_t>();
  ind = data.get<uint16_t>();
  reserved2 = data.get<uint16_t>();
  reserved3 = data.get<uint16_t>();

  if (2 == ind) { // ind is 2 for big images
    sensorLeftBorder = data.get<uint16_t>();
    sensorTopBorder = data.get<uint16_t>();
    sensorRightBorder = data.get<uint16_t>();
    sensorBottomBorder = data.get<uint16_t>();
    data.skipBytes(2 * sizeof(uint16_t));
    sensorBlackAreaLeft = data.get<uint16_t>();
    data.skipBytes(4 * sizeof(uint16_t));
    sensorBlackAreaTop = data.get<uint16_t>();
  }

  writeLog(DEBUG_PRIO_EXTRA,
           "IAD1 sensor width: %d, height: %d, crop: %u, %u, %u, %u, black "
           "area left: %u, top: %u",
           sensorWidth, sensorHeight, sensorLeftBorder, sensorTopBorder,
           sensorRightBorder, sensorBottomBorder, sensorBlackAreaLeft,
           sensorBlackAreaTop);

  // Validate.
  operator bool();
}

IsoMCanonIad1Box::operator bool() const {
  // No fields yet to validate, IAD1 is unused for decoding.
  return true; // OK!
}

CanonTimedMetadata::CanonTimedMetadata::Record::Record(ByteStream* bs) {
  assert(bs->getByteOrder() == Endianness::little);
  auto origPos = bs->getPosition();
  recSize = bs->getU32();
  recType = bs->getU16();
  reserved1 = bs->get<uint8_t>();
  reserved2 = bs->get<uint8_t>();
  reserved3 = bs->get<uint16_t>();
  reserved4 = bs->get<uint16_t>();
  payload = bs->getStream(recSize - (bs->getPosition() - origPos));
}

CanonTimedMetadata::CanonTimedMetadata(const ByteStream* bs) : data(*bs) {

  // CTMD is little-endian, force stream to correct endianness
  data.setByteOrder(Endianness::little);

  while (data.getRemainSize() > 0) {
    auto rec = Record(&data);
    // No record type can exists multiple times
    assert(records.find(rec.recType) == records.end());
    records[rec.recType] = rec;
  }
  assert(data.getRemainSize() == 0);
}

bool Cr3Decoder::isAppropriateDecoder(const IsoMRootBox& box) {
  return box.ftyp()->majorBrand == FourCharStr({'c', 'r', 'x', ' '});
}

RawImage Cr3Decoder::decodeRawInternal() {
  /*
  ByteStream biggestImage;

  for (const auto& track : rootBox->moov()->tracks) {
    for (const auto& chunk : track.mdia->minf->stbl->chunks) {
      if (chunk->getSize() > biggestImage.getSize())
        biggestImage = *chunk;
    }
  }
  */

  assert(crawBox);
  ByteStream biggestImage(
      *rootBox->moov()->tracks[2].mdia->minf->stbl->chunks[0]);

  // Setup image dimensions
  const auto& cmp1 = crawBox->CMP1();

  mRaw->dim = iPoint2D(cmp1->f_width, cmp1->f_height);
  mRaw->setCpp(1);
  mRaw->createData();

  assert(mRaw->getBpp() == 2);

  CrxDecompressor u(mRaw);
  u.decode(*cmp1, biggestImage);

  return mRaw;
}

bool Cr3Decoder::isCodecSupported(const std::string& compressorVersion) const {
  if (compressorVersion == "CanonHEIF001/10.00.00/00.00.00") {
    writeLog(DEBUG_PRIO_WARNING, "HEIF CNCV: '%s' is not supported",
             compressorVersion.c_str());
  }
  if (compressorVersion == "CanonCR3_001/01.09.00/01.00.00") {
    writeLog(DEBUG_PRIO_WARNING, "Raw-burst roll CNCV: '%s' is not supported",
             compressorVersion.c_str());
  }
  if (compressorVersion == "CanonCRM0001/02.09.00/00.00.00") {
    writeLog(DEBUG_PRIO_WARNING, "CRM movies CNCV: '%s' is not supported",
             compressorVersion.c_str());
  }

  return compressorVersion ==
             "CanonCR3_001/00.10.00/00.00.00" // EOS R5, R6 and 1DX Mark III
                                              // (raw)
         || compressorVersion ==
                "CanonCR3_002/00.10.00/00.00.00" // CR3 of 1DX Mark III (craw)
         || compressorVersion ==
                "CanonCR3_001/00.09.00/00.00.00"; // EOS R, EOS RP, M50, 250D,
                                                  // 90D, M6 Mark II, M200
}

void Cr3Decoder::checkSupportInternal(const CameraMetaData* meta) {
  // Get Canon UUID box and parse
  canonBox =
      std::make_unique<IsoMCanonBox>(rootBox->moov()->getBox(CanonBoxUuid));
  canonBox->parse();

  // Check compressor version string
  auto compressorVersion = canonBox->CNCV()->compressorVersion;
  writeLog(DEBUG_PRIO_EXTRA, "Compressor Version: %s",
           compressorVersion.c_str());
  if (!isCodecSupported(compressorVersion)) {
    ThrowRDE("CR3 compressor version (CNCV: %s) is not supported",
             compressorVersion.c_str());
  }

  // CMT1 contains a TIFF file with EXIF information
  auto camId = canonBox->CMT1()->mRootIFD0->getID();
  writeLog(DEBUG_PRIO_EXTRA, "CMT1 EXIF make: %s", camId.make.c_str());
  writeLog(DEBUG_PRIO_EXTRA, "CMT1 EXIF model: %s", camId.model.c_str());

  // Load CRAW box
  auto& stsd = rootBox->moov()->tracks[2].mdia->minf->stbl->stsd;
  crawBox = std::make_unique<IsoMCanonCrawBox>(stsd->dscs[0]);

  checkCameraSupported(meta, camId.make, camId.model, mode);
}

void Cr3Decoder::decodeMetaDataInternal(const CameraMetaData* meta) {
  const auto camId = canonBox->CMT1()->mRootIFD0->getID();

  uint32_t iso = 0;
  if (canonBox->CMT2()->mRootIFD0->hasEntryRecursive(ISOSPEEDRATINGS))
    iso = canonBox->CMT2()
              ->mRootIFD0->getEntryRecursive(ISOSPEEDRATINGS)
              ->getU32();

  // Big raw image is always in track 4
  assert(rootBox->moov()->tracks.size() >= 4);
  auto& track3Mdia = rootBox->moov()->tracks[3].mdia;

  // CTMD
  auto& CTMD_stsd = track3Mdia->minf->stbl->stsd;
  assert(CTMD_stsd->dscs.size() > 0);

  // Get Sample and rebuild a CTMD
  IsoMCanonTimedMetadataBox ctmd =
      IsoMCanonTimedMetadataBox(CTMD_stsd->dscs[0]);

  // CTMD MDAT
  assert(track3Mdia->minf->stbl->chunks.size() > 0);
  auto ctmd_chunk = track3Mdia->minf->stbl->chunks[0];

  Buffer ctmd_chunk_buf = ctmd_chunk->getSubView(0);

  auto ctmd_recs = CanonTimedMetadata(ctmd_chunk);

  // Record 8 contains EXIF data with CANONCOLORDATA tag
  auto rec8 = ctmd_recs.records[8].payload.getSubView(8);

  NORangesSet<Buffer> rs;

  // Rec. 8 contains TIFF data, but with corrupt IFD1 index. We
  // parse it manually.
  TiffRootIFD IFD_ctmd_rec8(nullptr, &rs, DataBuffer(rec8, Endianness::little),
                            8); // skip TIFF header

  if (IFD_ctmd_rec8.hasEntryRecursive(CANONCOLORDATA)) {
    TiffEntry* wb = IFD_ctmd_rec8.getEntryRecursive(CANONCOLORDATA);
    // this entry is a big table, and different cameras store used WB in
    // different parts, so find the offset, default is the most common one.
    // The wb_offset values in cameras.xml are extracted from:
    // https://github.com/exiftool/exiftool/blob/ceff3cbc4564e93518f3d2a2e00d8ae203ff54af/lib/Image/ExifTool/Canon.pm#L1910
    int offset = hints.get("wb_offset", 126);

    wb_coeffs[0] = static_cast<float>(wb->getU16(offset + 0)) / 1024.0;
    wb_coeffs[1] = static_cast<float>(wb->getU16(offset + 1)) / 1024.0;
    wb_coeffs[2] = 0; // GG
    wb_coeffs[3] = static_cast<float>(wb->getU16(offset + 3)) / 1024.0;

    writeLog(DEBUG_PRIO_EXTRA, "wb_coeffs:, 0: %f, 1: %f, 2: %f, 3: %f\n",
             wb_coeffs[0], wb_coeffs[1], wb_coeffs[2], wb_coeffs[3]);

  } else {
    writeLog(DEBUG_PRIO_EXTRA, "no wb_coeffs found");
  }

  // No CR3 camera has swapped_wb so far, but who knows...
  if (hints.has("swapped_wb")) {
    mRaw->metadata.wbCoeffs[0] = wb_coeffs[2];
    mRaw->metadata.wbCoeffs[1] = wb_coeffs[0];
    mRaw->metadata.wbCoeffs[2] = wb_coeffs[1];
  } else {
    mRaw->metadata.wbCoeffs[0] = wb_coeffs[0];
    mRaw->metadata.wbCoeffs[1] = wb_coeffs[1];
    mRaw->metadata.wbCoeffs[2] = wb_coeffs[3];
  }

  setMetaData(meta, camId.make, camId.model, mode, iso);

  // IAD1 described sensor constraints
  const auto& iad1 = crawBox->CDI1()->IAD1();

  if (mRaw->blackAreas.empty()) {
    mRaw->blackAreas.push_back(BlackArea(0, iad1->sensorBlackAreaLeft, true));
    mRaw->blackAreas.push_back(BlackArea(0, iad1->sensorBlackAreaTop, false));
  }

  if (applyCrop) {
    // IAD1 sensor parameters always crop one more pixel as needed.
    // We add back the missing pixels to match official specifications.
    iRectangle2D new_area(iad1->sensorLeftBorder, iad1->sensorTopBorder,
                          iad1->sensorRightBorder - iad1->sensorLeftBorder + 1,
                          iad1->sensorBottomBorder - iad1->sensorTopBorder + 1);

    mRaw->subFrame(new_area);
  }
}

} // namespace rawspeed
