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

#include "common/Common.h"               // for uint32
#include "io/ByteStream.h"               // for ByteStream
#include "io/Endianness.h"               // for getBE
#include "parsers/IsoMParserException.h" // for ThrowIPE
#include <array>                         // for array
#include <cassert>                       // for assert
#include <cstring>                       // for memcpy, strncpy
#include <initializer_list>              // for initializer_list
#include <memory>                        // for unique_ptr
#include <string>                        // for string
#include <utility>                       // for pair
#include <vector>                        // for vector

namespace rawspeed {

struct FourCharStr final {
  using value_type = uint32_t;
  static constexpr auto num_chars = sizeof(value_type) / sizeof(char);
  static_assert(num_chars == 4, "wanted 4 chars specifically");

  std::array<char, num_chars> data{};

  FourCharStr() = default;

  explicit constexpr FourCharStr(decltype(data) data_) : data(data_) {}

  explicit FourCharStr(value_type data_) {
    // Turn the unsigned integer into a 'string'
    data_ = getBE<value_type>(&data_);
    std::memcpy(data.data(), &data_, num_chars);
  }

  explicit operator std::string() const {
    return std::string(data.begin(), data.end());
  }

  std::string str() const { return static_cast<std::string>(*this); }
};
inline bool operator==(const FourCharStr& lhs, const FourCharStr& rhs) {
  return lhs.data == rhs.data;
}
inline bool operator!=(const FourCharStr& lhs, const FourCharStr& rhs) {
  return !operator==(lhs, rhs);
}

// The base foundation of the ISO Base Media File Format.

class IsoMRootBox;
struct IsoMMediaDataBox;

// The most basic box.
struct AbstractIsoMBox {
  typedef std::array<uint8_t, 16> UuidType;

  ByteStream data;

  FourCharStr boxType;
  UuidType userType{}; // when boxType == "uuid"

  AbstractIsoMBox() = default;

  explicit AbstractIsoMBox(ByteStream* bs);

  template <typename Box>
  static std::unique_ptr<Box> ParseBox(const AbstractIsoMBox& base,
                                       IsoMRootBox* root = nullptr) {
    auto box = std::make_unique<Box>(base);
    box->parse(root);
    return box;
  }
};

struct IsoMBoxTypes final {
  static constexpr FourCharStr ftyp = FourCharStr({'f', 't', 'y', 'p'});
  static constexpr FourCharStr co64 = FourCharStr({'c', 'o', '6', '4'});
  static constexpr FourCharStr stsz = FourCharStr({'s', 't', 's', 'z'});
  static constexpr FourCharStr stsc = FourCharStr({'s', 't', 's', 'c'});
  static constexpr FourCharStr stsd = FourCharStr({'s', 't', 's', 'd'});
  static constexpr FourCharStr stbl = FourCharStr({'s', 't', 'b', 'l'});
  static constexpr FourCharStr url = FourCharStr({'u', 'r', 'l', ' '});
  static constexpr FourCharStr dref = FourCharStr({'d', 'r', 'e', 'f'});
  static constexpr FourCharStr dinf = FourCharStr({'d', 'i', 'n', 'f'});
  static constexpr FourCharStr minf = FourCharStr({'m', 'i', 'n', 'f'});
  static constexpr FourCharStr mdia = FourCharStr({'m', 'd', 'i', 'a'});
  static constexpr FourCharStr trak = FourCharStr({'t', 'r', 'a', 'k'});
  static constexpr FourCharStr moov = FourCharStr({'m', 'o', 'o', 'v'});
  static constexpr FourCharStr mdat = FourCharStr({'m', 'd', 'a', 't'});

  static constexpr FourCharStr uuid = FourCharStr({'u', 'u', 'i', 'd'});
};

// The basic container.
class IsoMContainer {
  void lexBox();
  void lexSubBoxes();

protected:
  ByteStream cData;

  // These are specific for each container, and must be implemented.
  virtual void parseBox(const AbstractIsoMBox& box) = 0;
  virtual explicit operator bool() const = 0;

  friend struct IsoMMediaDataBox; // needs access to cData

public:
  std::vector<AbstractIsoMBox> boxes;

  IsoMContainer() = default;
  virtual ~IsoMContainer() = default;

  explicit IsoMContainer(ByteStream* bs);

  const AbstractIsoMBox& getBox(const AbstractIsoMBox::UuidType& uuid) const;

  // !!! DO NOT CALL FROM CONSTRUCTOR !!!
  void parse(IsoMRootBox* root = nullptr) {
    for (const auto& box : boxes)
      parseBox(box);
    operator bool();
  }
};

// No further boxes shall be constructible from ByteStream!

// The box that knows what it is.
template <const FourCharStr /* IsoMBoxTypes::* */& type>
struct IsoMBox : public AbstractIsoMBox {
  using BaseBox = IsoMBox<type>;

  static constexpr const FourCharStr /* IsoMBoxTypes::* */& BoxType = type;

  IsoMBox() = default;

  explicit IsoMBox(const AbstractIsoMBox& base) : AbstractIsoMBox(base) {
    assert(BoxType == boxType);
    if (BoxType != boxType) {
      ThrowIPE("Unexpected box type, got: '%s', expected: '%s'",
               BoxType.str().c_str(), boxType.str().c_str());
    }
  }
};

template <const FourCharStr /* IsoMBoxTypes::* */& type>
struct IsoMFullBox : public IsoMBox<type> {
  using BaseBox = IsoMBox<type>;

  uint8_t version;
  uint32_t flags : 24;

  uint8_t expectedVersion() const { return 0; }

  IsoMFullBox() = default;
  virtual ~IsoMFullBox() = default;

  explicit IsoMFullBox(const AbstractIsoMBox& base) : IsoMBox<type>(base) {
    // Highest 8 bits - version
    version = BaseBox::data.peekByte();
    // The rest, low 24 bits - flags
    flags = BaseBox::data.getU32() & ((1U << 24U) - 1U);

    if (expectedVersion() != version)
      ThrowIPE("Unexpected version of FullBox - %u", expectedVersion());
  }

  void parse(IsoMRootBox* root = nullptr) {}
};

template <const FourCharStr /* IsoMBoxTypes::* */& type>
class IsoMContainerBox : public IsoMBox<type>, public IsoMContainer {
public:
  using BaseBox = IsoMBox<type>;
  using BaseContainer = IsoMContainerBox<type>;

  IsoMContainerBox() = default;

  explicit IsoMContainerBox(const AbstractIsoMBox& base)
      : IsoMBox<type>(base), IsoMContainer(&(BaseBox::data)) {}
};


template <const FourCharStr /* IsoMBoxTypes::* */& type>
class IsoMContainerFullBox : public IsoMFullBox<type>, public IsoMContainer {
public:
  using BaseBox = IsoMFullBox<type>;
  using BaseContainer = IsoMContainerBox<type>;

  IsoMContainerFullBox() = default;

  explicit IsoMContainerFullBox(const AbstractIsoMBox& base)
      : IsoMFullBox<type>(base), IsoMContainer(&(BaseBox::data)) {}
};

// The actual boxes

struct IsoMFileTypeBox final : public IsoMBox<IsoMBoxTypes::ftyp> {
  static constexpr std::array<const FourCharStr, 1> supportedBrands = {
      FourCharStr({'c', 'r', 'x', ' '})};

  FourCharStr majorBrand;
  uint32_t minorVersion;
  std::vector<FourCharStr> compatibleBrands;

  explicit IsoMFileTypeBox(const AbstractIsoMBox& base);

  // Validate.
  explicit operator bool() const;

  void parse(IsoMRootBox* root = nullptr) {}
};

struct IsoMSampleDescriptionBox final : public IsoMFullBox<IsoMBoxTypes::stsd> {
  struct SampleEntry final : public AbstractIsoMBox {
    std::array<uint8_t, 6> reserved;
    uint16_t dataReferenceIndex;

    SampleEntry() = default;

    explicit SampleEntry(ByteStream* bs);
  };

  std::vector<SampleEntry> dscs;

  explicit IsoMSampleDescriptionBox(const AbstractIsoMBox& base);

  // Validate.
  explicit operator bool() const;
};

struct IsoMSampleToChunkBox final : public IsoMFullBox<IsoMBoxTypes::stsc> {
  struct Dsc final {
    uint32_t firstChunk;
    uint32_t samplesPerChunk;
    uint32_t sampleDescriptionIndex;
  };

  std::vector<Dsc> dscs;

  explicit IsoMSampleToChunkBox(const AbstractIsoMBox& base);

  // Validate.
  explicit operator bool() const;
};

struct IsoMSampleSizeBox final : public IsoMFullBox<IsoMBoxTypes::stsz> {
  std::vector<Buffer::size_type> chunkSizes;

  explicit IsoMSampleSizeBox(const AbstractIsoMBox& base);

  // Validate.
  explicit operator bool() const;
};

struct IsoMChunkLargeOffsetBox final : public IsoMFullBox<IsoMBoxTypes::co64> {
  std::vector<Buffer::size_type> chunkOffsets;

  explicit IsoMChunkLargeOffsetBox(const AbstractIsoMBox& base);

  // Validate.
  explicit operator bool() const;
};

class IsoMSampleTableBox final : public IsoMContainerBox<IsoMBoxTypes::stbl> {
  void parseBox(const AbstractIsoMBox& box) override;
  explicit operator bool() const override;

public:
  std::unique_ptr<IsoMSampleDescriptionBox> stsd;
  std::unique_ptr<IsoMSampleToChunkBox> stsc;
  std::unique_ptr<IsoMSampleSizeBox> stsz;
  std::unique_ptr<IsoMChunkLargeOffsetBox> co64;

  // will be filed by IsoMMediaDataBox::parse()
  std::vector<const ByteStream*> chunks;

  explicit IsoMSampleTableBox(const AbstractIsoMBox& base)
      : IsoMContainerBox(base) {}
};

struct IsoMDataReferenceBox final : public IsoMFullBox<IsoMBoxTypes::dref> {
  struct IsoMDataEntryUrlBox final : public IsoMFullBox<IsoMBoxTypes::url> {
    enum class Flags : decltype(IsoMFullBox::flags) {
      SelfContained = 0b1,
    };

    explicit IsoMDataEntryUrlBox(const AbstractIsoMBox& base);

    // Validate.
    explicit operator bool() const;
  };

  std::vector<IsoMDataEntryUrlBox> entries;

  explicit IsoMDataReferenceBox(const AbstractIsoMBox& base);

  // Validate.
  explicit operator bool() const;
};

class IsoMDataInformationBox final
    : public IsoMContainerBox<IsoMBoxTypes::dinf> {
  void parseBox(const AbstractIsoMBox& box) override;
  explicit operator bool() const override;

public:
  std::unique_ptr<IsoMDataReferenceBox> dref;

  explicit IsoMDataInformationBox(const AbstractIsoMBox& base)
      : IsoMContainerBox(base) {}
};

class IsoMMediaInformationBox final
    : public IsoMContainerBox<IsoMBoxTypes::minf> {
  void parseBox(const AbstractIsoMBox& box) override;
  explicit operator bool() const override;

public:
  std::unique_ptr<IsoMDataInformationBox> dinf;
  std::unique_ptr<IsoMSampleTableBox> stbl;

  explicit IsoMMediaInformationBox(const AbstractIsoMBox& base)
      : IsoMContainerBox(base) {}
};

class IsoMMediaBox final : public IsoMContainerBox<IsoMBoxTypes::mdia> {
  void parseBox(const AbstractIsoMBox& box) override;
  explicit operator bool() const override;

public:
  std::unique_ptr<IsoMMediaInformationBox> minf;

  explicit IsoMMediaBox(const AbstractIsoMBox& base) : IsoMContainerBox(base) {}
};

class IsoMTrackBox final : public IsoMContainerBox<IsoMBoxTypes::trak> {
  void parseBox(const AbstractIsoMBox& box) override;
  explicit operator bool() const override;

public:
  std::unique_ptr<IsoMMediaBox> mdia;

  explicit IsoMTrackBox(const AbstractIsoMBox& base) : IsoMContainerBox(base) {}
};

class IsoMMovieBox final : public IsoMContainerBox<IsoMBoxTypes::moov> {
  void parseBox(const AbstractIsoMBox& box) override;
  explicit operator bool() const override;

public:
  std::vector<IsoMTrackBox> tracks;

  explicit IsoMMovieBox(const AbstractIsoMBox& base) : IsoMContainerBox(base) {}
};

struct IsoMMediaDataBox final : public IsoMBox<IsoMBoxTypes::mdat> {
  explicit IsoMMediaDataBox(const AbstractIsoMBox& base)
      : BaseBox(base),
        mData(BaseBox::data.getStream(BaseBox::data.getRemainSize())) {}

  ByteStream mData;

  // The actual slicing of mData. Derived from SampleTable box.
  std::vector<ByteStream> chunks;

  // Validate.
  explicit operator bool() const;

  void parse(IsoMRootBox* root);
};

// The root box. It's just a container, and can only be created from ByteStream.

class IsoMRootBox final : public IsoMContainer {
  void parseBox(const AbstractIsoMBox& box) override;
  explicit operator bool() const override;

public:
  std::unique_ptr<IsoMFileTypeBox> ftypBox;
  std::unique_ptr<IsoMMovieBox> moovBox;
  std::unique_ptr<IsoMMediaDataBox> mdatBox;

  const std::unique_ptr<IsoMFileTypeBox>& ftyp() const;
  const std::unique_ptr<IsoMMovieBox>& moov() const;
  const std::unique_ptr<IsoMMediaDataBox>& mdat() const;

  explicit IsoMRootBox(ByteStream* bs) : IsoMContainer(bs) {}
};

} // namespace rawspeed
