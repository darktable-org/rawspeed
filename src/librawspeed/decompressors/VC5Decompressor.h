/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2018 Stefan Löffler

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
#include "common/Common.h"                      // for uint32
#include "common/RawImage.h"                    // for RawImageData
#include "common/SimpleLUT.h"                   // for SimpleLUT
#include "decompressors/AbstractDecompressor.h" // for AbstractDecompressor
#include "io/BitPumpMSB.h"                      // for BitPumpMSB
#include "io/ByteStream.h"                      // for ByteStream
#include <type_traits>                          // for underlying_type

namespace rawspeed {

const int MAX_NUM_PRESCALE = 8;

class ByteStream;
class RawImage;

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
  RawImage mImg;
  ByteStream mBs;

  static constexpr auto VC5_LOG_TABLE_BITWIDTH = 12;
  int outputBits;
  SimpleLUT<unsigned, VC5_LOG_TABLE_BITWIDTH> mVC5LogTable;

  void initVC5LogTable();

  static constexpr int numWaveletLevels = 3;
  static constexpr int numHighPassBands = 3;
  static constexpr int numSubbands = 1 + numHighPassBands * numWaveletLevels;

  struct {
    ushort16 iChannel;
    ushort16 iSubband;
    const ushort16 imgFormat = 4;
    const ushort16 patternWidth = 2;
    const ushort16 patternHeight = 2;
    const ushort16 cps = 1;
    ushort16 lowpassPrecision;
    short16 quantization;
  } mVC5;

  class Wavelet {
  public:
    uint16_t width, height;
    int16_t prescale;

    struct Band {
      ByteStream bs;
      std::vector<int16_t> data;
      int16_t quant; // only applicable for highpass bands.
      ushort16 lowpassPrecision; // only applicable for lowpass band.

      void decodeLowPassBand(const Wavelet& wavelet);
      void decodeHighPassBand(const Wavelet& wavelet);
    };
    static constexpr uint16_t numBands = 4;
    std::array<Band, numBands> bands;

    void setBandValid(int band);
    bool isBandValid(int band) const;
    uint32_t getValidBandMask() const { return mDecodedBandMask; }
    bool allBandsValid() const;

    void reconstructPass(Array2DRef<int16_t> dst, Array2DRef<int16_t> high,
                         Array2DRef<int16_t> low);

    void combineLowHighPass(Array2DRef<int16_t> dest, Array2DRef<int16_t> low,
                            Array2DRef<int16_t> high, int descaleShift,
                            bool clampUint /*= false*/);

    std::vector<int16_t> reconstructLowband(bool clampUint = false);

    Array2DRef<int16_t> bandAsArray2DRef(unsigned int iBand);

  protected:
    uint32 mDecodedBandMask = 0;

    static void dequantize(Array2DRef<int16_t> out, Array2DRef<int16_t> in,
                           int16_t quant);
  };

  struct Channel {
    std::array<Wavelet, numWaveletLevels> wavelets;

    std::vector<int16_t> data; // the final lowband.
    uint16_t width, height;
  };

  static constexpr int numChannels = 4;
  std::array<Channel, numChannels> channels;

  static void getRLV(BitPumpMSB* bits, int* value, unsigned int* count);

  void parseLargeCodeblock(const ByteStream& bs);

  // FIXME: this *should* be threadedable nicely.
  void combineFinalLowpassBands();

  void parseVC5();

public:
  VC5Decompressor(ByteStream bs, const RawImage& img);

  void decode(unsigned int offsetX, unsigned int offsetY, unsigned int width,
              unsigned int height);
};

} // namespace rawspeed
