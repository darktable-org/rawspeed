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

#pragma once

#include "common/RawImage.h"     // for RawImage
#include "decoders/RawDecoder.h" // for RawDecoder
#include "tiff/IsoMBox.h"        // for IsoMRootBox
#include <memory>                // for unique_ptr
#include <utility>               // for move

#include "tiff/TiffIFD.h"                           // for TiffRootIFDOwner

namespace rawspeed {

class CameraMetaData;

class Buffer;


struct IsoMBoxCanonTypes final {
  static constexpr FourCharStr CNCV = FourCharStr({'C', 'N', 'C', 'V'});
  static constexpr FourCharStr CCTP = FourCharStr({'C', 'C', 'T', 'P'});
  static constexpr FourCharStr CTBO = FourCharStr({'C', 'T', 'B', 'O'});
  static constexpr FourCharStr CMT1 = FourCharStr({'C', 'M', 'T', '1'});
  static constexpr FourCharStr CMT2 = FourCharStr({'C', 'M', 'T', '2'});
  static constexpr FourCharStr CMT3 = FourCharStr({'C', 'M', 'T', '3'});
  static constexpr FourCharStr CMT4 = FourCharStr({'C', 'M', 'T', '4'});
  static constexpr FourCharStr THMB = FourCharStr({'T', 'H', 'M', 'B'});

  static constexpr FourCharStr CRAW = FourCharStr({'C', 'R', 'A', 'W'});
  static constexpr FourCharStr CMP1 = FourCharStr({'C', 'M', 'P', '1'});
  static constexpr FourCharStr CDI1 = FourCharStr({'C', 'D', 'I', '1'});
  static constexpr FourCharStr IAD1 = FourCharStr({'I', 'A', 'D', '1'});

  static constexpr FourCharStr CTMD = FourCharStr({'C', 'T', 'M', 'D'});
};




class IsoMCanonCodecVersionBox final : public IsoMBox<IsoMBoxCanonTypes::CNCV> {
public:
  std::string compressorVersion;
  explicit IsoMCanonCodecVersionBox(const AbstractIsoMBox& base);
  void parse(IsoMRootBox* root = nullptr) {}
};


class IsoMCanonCTBOBox final : public IsoMBox<IsoMBoxCanonTypes::CTBO> {
public:
  explicit IsoMCanonCTBOBox(const AbstractIsoMBox& base) : IsoMBox(base) {}
  void parse(IsoMRootBox* root = nullptr) {}
};


class IsoMCanonCCTPBox final : public IsoMBox<IsoMBoxCanonTypes::CCTP> {
public:
  explicit IsoMCanonCCTPBox(const AbstractIsoMBox& base) : IsoMBox(base){}
  void parse(IsoMRootBox* root = nullptr) {}
};



class IsoMCanonCMT1Box final : public IsoMBox<IsoMBoxCanonTypes::CMT1> {
public:
  TiffRootIFDOwner mRootIFD0;

  explicit IsoMCanonCMT1Box(const AbstractIsoMBox& base);
  void parse(IsoMRootBox* root = nullptr) {}
};

class IsoMCanonCMT2Box final : public IsoMBox<IsoMBoxCanonTypes::CMT2> {
public:
  TiffRootIFDOwner mRootIFD0;

  explicit IsoMCanonCMT2Box(const AbstractIsoMBox& base);
  void parse(IsoMRootBox* root = nullptr) {}
};


class IsoMCanonCMT3Box final : public IsoMBox<IsoMBoxCanonTypes::CMT3> {
public:
  TiffRootIFDOwner mRootIFD0;

  explicit IsoMCanonCMT3Box(const AbstractIsoMBox& base);
  void parse(IsoMRootBox* root = nullptr) {}
};

class IsoMCanonCMT4Box final : public IsoMBox<IsoMBoxCanonTypes::CMT4> {
public:
  TiffRootIFDOwner mRootIFD0;

  explicit IsoMCanonCMT4Box(const AbstractIsoMBox& base);
  void parse(IsoMRootBox* root = nullptr) {}
};

class IsoMCanonThumbnailBox final : public IsoMBox<IsoMBoxCanonTypes::THMB> {
public:
  explicit IsoMCanonThumbnailBox(const AbstractIsoMBox& base) : IsoMBox(base) {}
  void parse(IsoMRootBox* root = nullptr) {}
};





class IsoMCanonBox final : public IsoMContainerBox<IsoMBoxTypes::uuid> {
  void parseBox(const AbstractIsoMBox& box) override;
  explicit operator bool() const override;

  std::unique_ptr<IsoMCanonCodecVersionBox> cncvBox;
  std::unique_ptr<IsoMCanonCCTPBox> cctpBox;
  std::unique_ptr<IsoMCanonCTBOBox> ctboBox;
  std::unique_ptr<IsoMCanonCMT1Box> cmt1Box;
  std::unique_ptr<IsoMCanonCMT2Box> cmt2Box;
  std::unique_ptr<IsoMCanonCMT3Box> cmt3Box;
  std::unique_ptr<IsoMCanonCMT4Box> cmt4Box;
  std::unique_ptr<IsoMCanonThumbnailBox> thmbBox;

public:
  explicit IsoMCanonBox(const AbstractIsoMBox& base)
      : IsoMContainerBox(base) {}

  const std::unique_ptr<IsoMCanonCodecVersionBox>& CNCV() const;
  const std::unique_ptr<IsoMCanonCCTPBox>& CCTP() const;
  const std::unique_ptr<IsoMCanonCTBOBox>& CTBO() const;
  const std::unique_ptr<IsoMCanonCMT1Box>& CMT1() const;
  const std::unique_ptr<IsoMCanonCMT2Box>& CMT2() const;
  const std::unique_ptr<IsoMCanonCMT3Box>& CMT3() const;
  const std::unique_ptr<IsoMCanonCMT4Box>& CMT4() const;
  const std::unique_ptr<IsoMCanonThumbnailBox>& THMB() const;
};







class IsoMCanonCmp1Box final : public IsoMBox<IsoMBoxCanonTypes::CMP1> {
  //void parseBox(const AbstractIsoMBox& box) override;
  explicit operator bool() const;

public:
  uint16_t reserved1; // unknown, -1?
  uint16_t headerSize;
  int16_t version;
  int16_t versionSub; // Always 00 00
  int32_t f_width;
  int32_t f_height;
  int32_t tileWidth;
  int32_t tileHeight;
  int32_t nBits;
  int32_t nPlanes;
  int32_t cfaLayout;
  int32_t encType;
  int32_t imageLevels;
  int32_t hasTileCols;
  int32_t hasTileRows;
  int32_t mdatHdrSize;
  int32_t reserved2; // unknown
  std::array<uint8_t, 16> reserved3; // unknown

  explicit IsoMCanonCmp1Box(const AbstractIsoMBox& base);
};




class IsoMCanonIad1Box final : public IsoMFullBox<IsoMBoxCanonTypes::IAD1> {
  //void parseBox(const AbstractIsoMBox& box) override;
  explicit operator bool() const;

public:
  // IAD1 data is not required to decode the image.
  // We skip parsing IAD1.

  uint16_t sensorWidth;
  uint16_t sensorHeight;
  uint16_t reserved1;
  uint16_t ind; // 0 = small, 1 = big
  uint16_t reserved2;
  uint16_t reserved3;
  // Big image flags (we ignore small flags, not needed for decoding)
  uint16_t sensorLeftBorder;
  uint16_t sensorTopBorder;
  uint16_t sensorRightBorder;
  uint16_t sensorBottomBorder;
  // followed by more unknown fields
  uint16_t sensorBlackAreaLeft;
  // followed by more unknown fields
  uint16_t sensorBlackAreaTop;
  // followed by more unknown fields

  explicit IsoMCanonIad1Box(const AbstractIsoMBox& base);
};



class IsoMCanonCdi1Box final : public IsoMContainerFullBox<IsoMBoxCanonTypes::CDI1> {
  void parseBox(const AbstractIsoMBox& box) override;
  explicit operator bool() const override;
public:
  std::unique_ptr<IsoMCanonIad1Box> iad1Box;
  explicit IsoMCanonCdi1Box(const AbstractIsoMBox& base)
    : IsoMContainerFullBox<IsoMBoxCanonTypes::CDI1>(base) {
    }

  const std::unique_ptr<IsoMCanonIad1Box>& IAD1() const;
};


// This is derived from SampleEntry
class IsoMCanonCrawBox final : public IsoMBox<IsoMBoxCanonTypes::CRAW> {
  //void parseBox(const AbstractIsoMBox& box) override;
  explicit operator bool() const;

  std::unique_ptr<IsoMCanonCmp1Box> cmp1Box;
  std::unique_ptr<IsoMCanonCdi1Box> cdi1Box;

public:
  std::array<uint8_t, 6> reserved1;
  uint16_t dataReferenceIndex;
  std::array<uint8_t, 16> reserved2; // unknown, all zero
  uint16_t width;
  uint16_t height;
  uint32_t xResolution; // stored as 0072 0000 fixed point
  uint32_t yResolution; // stored as 0072 0000 fixed point
  uint32_t reserved3; // unknown
  uint16_t reserved4; // unknown
  std::array<uint8_t, 32> reserved5; // unknown
  uint16_t bitDepth;
  uint16_t reserved6; // unknown
  uint16_t flags; // unknown, 3 for Jpeg, 1 for craw/raw
  uint16_t formatInd; // 0 for jpeg, 1 for craw/raw

  explicit IsoMCanonCrawBox(const AbstractIsoMBox& base);

  const std::unique_ptr<IsoMCanonCmp1Box>& CMP1() const;
  const std::unique_ptr<IsoMCanonCdi1Box>& CDI1() const;
};





// Derived from SampleEntry
class IsoMCanonTimedMetadataBox final : public IsoMBox<IsoMBoxCanonTypes::CTMD> {
  struct RecordDesc final {
    uint32_t recType;
    uint32_t recSize;

    RecordDesc() = default;

    explicit RecordDesc(ByteStream* bs);
  };

  //void parseBox(const AbstractIsoMBox& box) override;
  explicit operator bool() const;

public:
  std::array<uint8_t, 6> reserved1;
  uint16_t dataReferenceIndex;
  std::vector<RecordDesc> recDescs;

  explicit IsoMCanonTimedMetadataBox(const AbstractIsoMBox& base);

};







// TODO: List of records?
class CanonTimedMetadata final {
  struct Record final {
    uint32_t recSize;
    uint16_t recType;
    uint8_t reserved1;
    uint8_t reserved2;
    uint16_t reserved3;
    uint16_t reserved4;
    ByteStream payload;

    Record() = default;

    explicit Record(ByteStream* bs);
  };

  ByteStream data;

  //void parseBox(const AbstractIsoMBox& box) override;
  explicit operator bool() const;

public:
  std::map<uint32_t, Record> records; // type + record

  explicit CanonTimedMetadata(const ByteStream* bs);

};













class Cr3Decoder final : public RawDecoder {
  std::unique_ptr<const IsoMRootBox> rootBox;
  std::unique_ptr<IsoMCanonBox> canonBox;
  std::unique_ptr<const IsoMCanonCrawBox> crawBox;

  std::array<float, 4> wb_coeffs = {{NAN, NAN, NAN, NAN}};

  std::string mode;

public:
  static bool isAppropriateDecoder(const IsoMRootBox& box);

  Cr3Decoder(std::unique_ptr<const IsoMRootBox> rootBox_, const Buffer* file)
      : RawDecoder(file), rootBox(std::move(rootBox_)) {}

  RawImage decodeRawInternal() override;
  void checkSupportInternal(const CameraMetaData* meta) override;
  void decodeMetaDataInternal(const CameraMetaData* meta) override;

protected:
  int getDecoderVersion() const override { return 0; }
  bool isCodecSupported(const std::string& compressorVersion) const;
};

} // namespace rawspeed
