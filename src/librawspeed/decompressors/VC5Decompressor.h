/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2018 Stefan Löffler
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

#include "common/Array2DRef.h"                  // for Array2DRef
#include "common/DefaultInitAllocatorAdaptor.h" // for DefaultInitAllocator...
#include "common/RawImage.h"                    // for RawImage
#include "common/SimpleLUT.h"                   // for SimpleLUT, SimpleLUT...
#include "decompressors/AbstractDecompressor.h" // for AbstractDecompressor
#include "io/BitPumpMSB.h"                      // for BitPumpMSB
#include "io/ByteStream.h"                      // for ByteStream
#include <array>                                // for array
#include <cstdint>                              // for int16_t, uint16_t
#include <memory>                               // for unique_ptr
#include <optional>                             // for optional
#include <type_traits>                          // for __underlying_type_im...
#include <utility>                              // for move
#include <vector>                               // for vector

namespace rawspeed {

const int MAX_NUM_PRESCALE = 8;

// Decompresses VC-5 as used by GoPro

enum class VC5Tag : int16_t {
  NoTag = 0x0, // synthetic, not an actual tag

  ChannelCount = 0x000c,
  ImageWidth = 0x0014,
  ImageHeight = 0x0015,
  LowpassPrecision = 0x0023,
  SubbandCount = 0x000E,
  SubbandNumber = 0x0030,
  Quantization = 0x0035,
  ChannelNumber = 0x003e,
  ImageFormat = 0x0054,
  MaxBitsPerComponent = 0x0066,
  PatternWidth = 0x006a,
  PatternHeight = 0x006b,
  ComponentsPerSample = 0x006c,
  PrescaleShift = 0x006d,

  LARGE_CHUNK = 0x2000,
  SMALL_CHUNK = 0x4000,
  UniqueImageIdentifier = 0x4004,
  LargeCodeblock = 0x6000,

  Optional = int16_t(0x8000U), // only signbit set
};
inline VC5Tag operator&(VC5Tag LHS, VC5Tag RHS) {
  using value_type = std::underlying_type<VC5Tag>::type;
  return static_cast<VC5Tag>(static_cast<value_type>(LHS) &
                             static_cast<value_type>(RHS));
}
inline bool matches(VC5Tag LHS, VC5Tag RHS) {
  // Are there any common bit set?
  return (LHS & RHS) != VC5Tag::NoTag;
}
inline bool is(VC5Tag LHS, VC5Tag RHS) {
  // Does LHS have all the RHS bits set?
  return (LHS & RHS) == RHS;
}
inline VC5Tag operator-(VC5Tag tag) {
  using value_type = std::underlying_type<VC5Tag>::type;
  // Negate
  return static_cast<VC5Tag>(-static_cast<value_type>(tag));
}

class VC5Decompressor final : public AbstractDecompressor {
  RawImage mRaw;
  ByteStream mBs;

  static constexpr auto VC5_LOG_TABLE_BITWIDTH = 12;
  int outputBits;
  SimpleLUT<unsigned, VC5_LOG_TABLE_BITWIDTH> mVC5LogTable;

  void initVC5LogTable();

  static constexpr int numWaveletLevels = 3;
  static constexpr int numHighPassBands = 3;
  static constexpr int numLowPassBands = 1;
  static constexpr int numSubbands =
      numLowPassBands + numHighPassBands * numWaveletLevels;

  struct {
    uint16_t iChannel = 0; // 0'th channel is the default
    std::optional<uint16_t> iSubband;
    std::optional<uint16_t> lowpassPrecision;
    std::optional<int16_t> quantization;

    const uint16_t imgFormat = 4;
    const uint16_t patternWidth = 2;
    const uint16_t patternHeight = 2;
    const uint16_t cps = 1;
  } mVC5;

  class Wavelet {
  public:
    int width, height;
    int16_t prescale;

    struct AbstractBand {
      const Wavelet& wavelet;
      std::vector<int16_t, DefaultInitAllocatorAdaptor<int16_t>> data_storage;
      Array2DRef<int16_t> data;
      explicit AbstractBand(const Wavelet& wavelet_) : wavelet(wavelet_) {}
      virtual ~AbstractBand() = default;
      virtual void decode() = 0;
    };
    struct ReconstructableBand final : AbstractBand {
      bool clampUint;
      std::vector<int16_t, DefaultInitAllocatorAdaptor<int16_t>>
          lowpass_storage;
      Array2DRef<int16_t> lowpass;
      std::vector<int16_t, DefaultInitAllocatorAdaptor<int16_t>>
          highpass_storage;
      Array2DRef<int16_t> highpass;
      explicit ReconstructableBand(const Wavelet& wavelet_,
                                   bool clampUint_ = false)
          : AbstractBand(wavelet_), clampUint(clampUint_) {}
      void processLow() noexcept;
      void processHigh() noexcept;
      void combine() noexcept;
      void decode() noexcept final;
    };
    struct AbstractDecodeableBand : AbstractBand {
      ByteStream bs;
      explicit AbstractDecodeableBand(const Wavelet& wavelet_, ByteStream bs_)
          : AbstractBand(wavelet_), bs(std::move(bs_)) {}
    };
    struct LowPassBand final : AbstractDecodeableBand {
      uint16_t lowpassPrecision;
      LowPassBand(const Wavelet& wavelet_, ByteStream bs_,
                  uint16_t lowpassPrecision_);
      void decode() final;
    };
    struct HighPassBand final : AbstractDecodeableBand {
      int16_t quant;
      HighPassBand(const Wavelet& wavelet_, ByteStream bs_, int16_t quant_)
          : AbstractDecodeableBand(wavelet_, std::move(bs_)), quant(quant_) {}
      void decode() final;
    };

    static constexpr uint16_t maxBands = 4;
    std::vector<std::unique_ptr<AbstractBand>> bands;

    void clear() {
      for (auto& band : bands)
        band.reset();
    }

    void setBandValid(int band);
    [[nodiscard]] bool isBandValid(int band) const;
    [[nodiscard]] uint32_t getValidBandMask() const { return mDecodedBandMask; }
    [[nodiscard]] bool allBandsValid() const;

    void reconstructPass(Array2DRef<int16_t> dst,
                         Array2DRef<const int16_t> high,
                         Array2DRef<const int16_t> low) const noexcept;

    void combineLowHighPass(Array2DRef<int16_t> dst,
                            Array2DRef<const int16_t> low,
                            Array2DRef<const int16_t> high, int descaleShift,
                            bool clampUint /*= false*/) const noexcept;

    [[nodiscard]] Array2DRef<const int16_t>
    bandAsArray2DRef(unsigned int iBand) const;

  protected:
    uint32_t mDecodedBandMask = 0;
  };

  struct Channel {
    std::array<Wavelet, numWaveletLevels + 1> wavelets;
  };

  static constexpr int numChannels = 4;
  static constexpr int numSubbandsTotal = numSubbands * numChannels;
  static constexpr int numLowPassBandsTotal = numWaveletLevels * numChannels;
  std::array<Channel, numChannels> channels;

  struct DecodeableBand {
    Wavelet::AbstractDecodeableBand& band;
    const Wavelet& wavelet;
    DecodeableBand(Wavelet::AbstractDecodeableBand& band_,
                   const Wavelet& wavelet_)
        : band(band_), wavelet(wavelet_) {}
  };
  std::vector<DecodeableBand> allDecodeableBands;

  struct ReconstructionStep {
    Wavelet& wavelet;
    Wavelet::ReconstructableBand& band;
    ReconstructionStep(Wavelet& wavelet_, Wavelet::ReconstructableBand& band_)
        : wavelet(wavelet_), band(band_) {}
  };
  std::vector<ReconstructionStep> reconstructionSteps;

  static inline std::pair<int16_t /*value*/, unsigned int /*count*/>
  getRLV(BitPumpMSB& bits);

  void parseLargeCodeblock(const ByteStream& bs);

  void prepareBandDecodingPlan();
  void prepareBandReconstruction();
  void prepareDecodingPlan();

  void decodeBands(bool* exceptionThrown) const noexcept;

  void reconstructLowpassBands() const noexcept;

  void combineFinalLowpassBands() const noexcept;

  void decodeThread(bool* exceptionThrown) const noexcept;

  void parseVC5();

public:
  VC5Decompressor(ByteStream bs, const RawImage& img);

  void decode(unsigned int offsetX, unsigned int offsetY, unsigned int width,
              unsigned int height);
};

} // namespace rawspeed
