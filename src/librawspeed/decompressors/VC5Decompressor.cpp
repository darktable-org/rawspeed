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

/*
  This is a decompressor for VC-5 raw compression algo, as used in GoPro raws.
  This implementation is similar to that one of the official reference
  implementation of the https://github.com/gopro/gpr project, and is producing
  bitwise-identical output as compared with the Adobe DNG Converter
  implementation.
 */

#include "decompressors/VC5Decompressor.h"
#include "common/Array2DRef.h"            // for Array2DRef
#include "common/SimpleLUT.h"             // for SimpleLUT
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include <cmath>
#include <limits> // for numeric_limits
#include <utility>

// Definitions needed by table17.inc
// Taken from
// https://github.com/gopro/gpr/blob/a513701afce7b03173213a2f67dfd9dd28fa1868/source/lib/vc5_decoder/vlc.h
struct RLV {
  uint_fast8_t size; //!< Size of code word in bits
  uint32_t bits;     //!< Code word bits right justified
  uint32_t count;    //!< Run length
  int32_t value;     //!< Run value (unsigned)
};
#define RLVTABLE(n)                                                            \
  static struct {                                                              \
    uint32_t length;                                                           \
    RLV entries[n];                                                            \
  }
#include "common/table17.inc"

#define VC5_TAG_ChannelCount 0x000c
#define VC5_TAG_ImageWidth 0x0014
#define VC5_TAG_ImageHeight 0x0015
#define VC5_TAG_LowpassPrecision 0x0023
#define VC5_TAG_SubbandCount 0x000E
#define VC5_TAG_SubbandNumber 0x0030
#define VC5_TAG_Quantization 0x0035
#define VC5_TAG_ChannelNumber 0x003e
#define VC5_TAG_ImageFormat 0x0054
#define VC5_TAG_MaxBitsPerComponent 0x0066
#define VC5_TAG_PatternWidth 0x006a
#define VC5_TAG_PatternHeight 0x006b
#define VC5_TAG_ComponentsPerSample 0x006c
#define VC5_TAG_PrescaleShift 0x006d

#define VC5_TAG_LARGE_CHUNK 0x2000
#define VC5_TAG_SMALL_CHUNK 0x4000
#define VC5_TAG_UniqueImageIdentifier 0x4004
#define VC5_TAG_LargeCodeblock 0x6000

#define PRECISION_MIN 8
#define PRECISION_MAX 32

#define MARKER_BAND_END 1

namespace rawspeed {

void VC5Decompressor::Wavelet::initialize(uint16_t waveletWidth,
                                          uint16_t waveletHeight) {
  this->width = waveletWidth;
  this->height = waveletHeight;
  mDecodedBandMask = 0;

  for (auto& band : bands)
    band.data.resize(waveletWidth * waveletHeight);

  mInitialized = true;
}

void VC5Decompressor::Wavelet::setBandValid(const int band) {
  mDecodedBandMask |= (1 << band);
}

bool VC5Decompressor::Wavelet::isBandValid(const int band) const {
  return mDecodedBandMask & (1 << band);
}

bool VC5Decompressor::Wavelet::allBandsValid() const {
  return mDecodedBandMask == static_cast<uint32>((1 << numBands) - 1);
}

Array2DRef<int16_t>
VC5Decompressor::Wavelet::bandAsArray2DRef(const unsigned int iBand) {
  return {bands[iBand].data.data(), width, height};
}

void VC5Decompressor::Wavelet::clear() {
  for (auto& band : bands) {
    band.data.clear();
    band.data.shrink_to_fit();
  }
  mInitialized = false;
}

// static
void VC5Decompressor::Wavelet::dequantize(Array2DRef<int16_t> out,
                                          Array2DRef<int16_t> in,
                                          int16_t quant) {
  auto dequantize = [quant](int16_t val) -> int16_t {
    double c = val;
    // Invert companding curve
    c += (c * c * c * 768) / (255. * 255. * 255.);
    return static_cast<int16_t>(c) * quant;
  };

  // FIXME: could use the SimpleLUT,
  // should be profitable if  in.height * in.width > UINT16_MAX,
  // and table lookup is faster than that computation.

  for (unsigned int y = 0; y < in.height; ++y) {
    for (unsigned int x = 0; x < in.width; ++x)
      out(x, y) = dequantize(in(x, y));
  }
}

namespace {
auto convolute = [](unsigned x, unsigned y, std::array<int, 4> muls,
                    Array2DRef<int16_t> high, auto lowGetter,
                    int DescaleShift = 0) {
  auto highCombined = muls[0] * high(x, y);
  auto lowsCombined = [muls, lowGetter]() {
    int lows = 0;
    for (int i = 0; i < 3; i++)
      lows += muls[1 + i] * lowGetter(i);
    return lows;
  }();
  // Round up 'lows' up
  lowsCombined += 4;
  // And finally 'average' them.
  auto lowsRounded = lowsCombined >> 3;
  auto total = highCombined + lowsRounded;
  // Descale it.
  total <<= DescaleShift;
  // And average it.
  total >>= 1;
  return total;
};
} // namespace

void VC5Decompressor::Wavelet::reconstructPass(Array2DRef<int16_t> dst,
                                               Array2DRef<int16_t> high,
                                               Array2DRef<int16_t> low) {
  unsigned int x, y;

  auto convolution = [&x, &y, high](std::array<int, 4> muls, auto lowGetter) {
    return convolute(x, y, muls, high, lowGetter, /*DescaleShift*/ 0);
  };

  // Vertical reconstruction
  // 1st row
  y = 0;
  for (x = 0; x < width; ++x) {
    auto getter = [&x, &y, low](int delta) { return low(x, y + delta); };

    static constexpr std::array<int, 4> even_muls = {+1, +11, -4, +1};
    int even = convolution(even_muls, getter);
    static constexpr std::array<int, 4> odd_muls = {-1, +5, +4, -1};
    int odd = convolution(odd_muls, getter);

    dst(x, 2 * y) = static_cast<int16_t>(even);
    dst(x, 2 * y + 1) = static_cast<int16_t>(odd);
  }
  // middle rows
  for (y = 1; y + 1 < height; ++y) {
    for (x = 0; x < width; ++x) {
      auto getter = [&x, &y, low](int delta) { return low(x, y - 1 + delta); };

      static constexpr std::array<int, 4> even_muls = {+1, +1, +8, -1};
      int even = convolution(even_muls, getter);
      static constexpr std::array<int, 4> odd_muls = {-1, -1, +8, +1};
      int odd = convolution(odd_muls, getter);

      dst(x, 2 * y) = static_cast<int16_t>(even);
      dst(x, 2 * y + 1) = static_cast<int16_t>(odd);
    }
  }
  // last row
  for (x = 0; x < width; ++x) {
    auto getter = [&x, &y, low](int delta) { return low(x, y - delta); };

    static constexpr std::array<int, 4> even_muls = {+1, +5, +4, -1};
    int even = convolution(even_muls, getter);
    static constexpr std::array<int, 4> odd_muls = {-1, +11, -4, +1};
    int odd = convolution(odd_muls, getter);

    dst(x, 2 * y) = static_cast<int16_t>(even);
    dst(x, 2 * y + 1) = static_cast<int16_t>(odd);
  }
}

void VC5Decompressor::Wavelet::combineLowHighPass(Array2DRef<int16_t> dest,
                                                  Array2DRef<int16_t> low,
                                                  Array2DRef<int16_t> high,
                                                  int descaleShift,
                                                  bool clampUint = false) {
  unsigned int x, y;

  auto convolution = [&x, &y, high, descaleShift](std::array<int, 4> muls,
                                                  auto lowGetter) {
    return convolute(x, y, muls, high, lowGetter, descaleShift);
  };

  // Horizontal reconstruction
  for (y = 0; y < dest.height; ++y) {
    x = 0;

    // First col

    auto getter_first = [&x, &y, low](int delta) { return low(x + delta, y); };

    static constexpr std::array<int, 4> even_muls = {+1, +11, -4, +1};
    int even = convolution(even_muls, getter_first);
    static constexpr std::array<int, 4> odd_muls = {-1, +5, +4, -1};
    int odd = convolution(odd_muls, getter_first);

    if (clampUint) {
      even = clampBits(even, 14);
      odd = clampBits(odd, 14);
    }
    dest(2 * x, y) = static_cast<int16_t>(even);
    dest(2 * x + 1, y) = static_cast<int16_t>(odd);

    // middle cols
    for (x = 1; x + 1 < width; ++x) {
      auto getter = [&x, &y, low](int delta) { return low(x - 1 + delta, y); };

      static constexpr std::array<int, 4> middle_even_muls = {+1, +1, +8, -1};
      even = convolution(middle_even_muls, getter);
      static constexpr std::array<int, 4> middle_odd_muls = {-1, -1, +8, +1};
      odd = convolution(middle_odd_muls, getter);

      if (clampUint) {
        even = clampBits(even, 14);
        odd = clampBits(odd, 14);
      }
      dest(2 * x, y) = static_cast<int16_t>(even);
      dest(2 * x + 1, y) = static_cast<int16_t>(odd);
    }

    // last col

    auto getter_last = [&x, &y, low](int delta) { return low(x - delta, y); };

    static constexpr std::array<int, 4> last_even_muls = {+1, +5, +4, -1};
    even = convolution(last_even_muls, getter_last);
    static constexpr std::array<int, 4> last_odd_muls = {-1, +11, -4, +1};
    odd = convolution(last_odd_muls, getter_last);

    if (clampUint) {
      even = clampBits(even, 14);
      odd = clampBits(odd, 14);
    }
    dest(2 * x, y) = static_cast<int16_t>(even);
    dest(2 * x + 1, y) = static_cast<int16_t>(odd);
  }
}

std::vector<int16_t> VC5Decompressor::Wavelet::reconstructLowband(
    const int16_t prescale, const bool clampUint /* = false */) {
  int16_t descaleShift = (prescale == 2 ? 2 : 0);
  // Assert valid quantization values
  if (bands[0].quant == 0)
    bands[0].quant = 1;
  for (int i = 0; i < numBands; ++i) {
    if (bands[i].quant == 0)
      ThrowRDE("Quant value of band %i must not be zero", i);
  }

  std::vector<int16_t> lowpass_storage =
      Array2DRef<int16_t>::create(width, 2 * height);
  std::vector<int16_t> highpass_storage =
      Array2DRef<int16_t>::create(width, 2 * height);

  Array2DRef<int16_t> lowpass(lowpass_storage.data(), width, 2 * height);
  Array2DRef<int16_t> highpass(highpass_storage.data(), width, 2 * height);

  {
    std::vector<int16_t> lowhigh_storage =
        Array2DRef<int16_t>::create(width, height);
    std::vector<int16_t> highlow_storage =
        Array2DRef<int16_t>::create(width, height);
    std::vector<int16_t> highhigh_storage =
        Array2DRef<int16_t>::create(width, height);

    Array2DRef<int16_t> lowlow = bandAsArray2DRef(0);
    Array2DRef<int16_t> lowhigh(lowhigh_storage.data(), width, height);
    Array2DRef<int16_t> highlow(highlow_storage.data(), width, height);
    Array2DRef<int16_t> highhigh(highhigh_storage.data(), width, height);

    dequantize(lowhigh, bandAsArray2DRef(1), bands[1].quant);
    dequantize(highlow, bandAsArray2DRef(2), bands[2].quant);
    dequantize(highhigh, bandAsArray2DRef(3), bands[3].quant);

    // Reconstruct the "immediates", the actual low pass ...
    reconstructPass(lowpass, highlow, lowlow);
    // ... and high pass.
    reconstructPass(highpass, highhigh, lowhigh);
  }

  std::vector<int16_t> dest_storage =
      Array2DRef<int16_t>::create(2 * width, 2 * height);
  Array2DRef<int16_t> dest(dest_storage.data(), 2 * width, 2 * height);

  // And finally, combine the low pass, and high pass.
  combineLowHighPass(dest, lowpass, highpass, descaleShift, clampUint);

  return dest_storage;
}

VC5Decompressor::VC5Decompressor(ByteStream bs, const RawImage& img)
    : AbstractDecompressor(), mImg(img), mBs(std::move(bs)) {
  mVC5.iChannel = 0;
  mVC5.iSubband = 0;
  mVC5.imgWidth = 0;
  mVC5.imgHeight = 0;
  mVC5.imgFormat = 4;
  mVC5.patternWidth = 2;
  mVC5.patternHeight = 2;
  mVC5.cps = 0;
  mVC5.bpc = 0;
  mVC5.lowpassPrecision = 0;
  mVC5.quantization = 0;

  int outputBits = 0;
  for (int wp = img->whitePoint; wp != 0; wp >>= 1)
    ++outputBits;
  assert(outputBits <= 16);

  mVC5LogTable =
      decltype(mVC5LogTable)([outputBits](unsigned i, unsigned tableSize) {
        // The vanilla "inverse log" curve for decoding.
        auto normalizedCurve = [](auto normalizedI) {
          return (std::pow(113.0, normalizedI) - 1) / 112.0;
        };

        auto normalizeI = [tableSize](auto x) { return x / (tableSize - 1.0); };
        auto denormalizeY = [maxVal = std::numeric_limits<ushort16>::max()](
                                auto y) { return maxVal * y; };
        // Adjust for output whitelevel bitdepth.
        auto rescaleY = [outputBits](auto y) {
          auto scale = 16 - outputBits;
          return y >> scale;
        };

        const auto naiveY = denormalizeY(normalizedCurve(normalizeI(i)));
        const auto intY = static_cast<unsigned int>(naiveY);
        const auto rescaledY = rescaleY(intY);
        return rescaledY;
      });
}

void VC5Decompressor::decode(unsigned int offsetX, unsigned int offsetY) {
  if (offsetX || offsetY)
    ThrowRDE("VC5Decompressor expects to fill the whole image, not some tile.");

  mBs.setByteOrder(Endianness::big);

  assert(mImg->dim.x > 0);
  assert(mImg->dim.y > 0);

  // All VC-5 data must start with "VC-%" (0x56432d35)
  if (mBs.getU32() != 0x56432d35)
    ThrowRDE("not a valid VC-5 datablock");

  bool done = false;
  while (!done) {
    auto tag = static_cast<int16_t>(mBs.getU16());
    ushort16 val = mBs.getU16();

    bool optional;
    if (tag < 0) {
      tag = -tag;
      optional = true;
    } else
      optional = false;

    switch (tag) {
    case VC5_TAG_ChannelCount:
      if (val != numChannels)
        ThrowRDE("Bad channel count %u, expected %u", val, numChannels);
      break;
    case VC5_TAG_ImageWidth:
      mVC5.imgWidth = val;
      break;
    case VC5_TAG_ImageHeight:
      mVC5.imgHeight = val;
      break;
    case VC5_TAG_LowpassPrecision:
      if (val < PRECISION_MIN || val > PRECISION_MAX)
        ThrowRDE("Invalid precision %i", val);
      mVC5.lowpassPrecision = val;
      break;
    case VC5_TAG_ChannelNumber:
      if (val >= numChannels)
        ThrowRDE("Bad channel number (%u)", val);
      mVC5.iChannel = val;
      break;
    case VC5_TAG_ImageFormat:
      if (val != 4)
        ThrowRDE("Image format %i is not 4(RAW)", val);
      mVC5.imgFormat = val; // 4=RAW
      break;
    case VC5_TAG_SubbandCount:
      if (val != numSubbands)
        ThrowRDE("Unexpected subband count %u, expected %u", val, numSubbands);
      break;
    case VC5_TAG_MaxBitsPerComponent:
      mVC5.bpc = val;
      break;
    case VC5_TAG_PatternWidth:
      mVC5.patternWidth = val;
      break;
    case VC5_TAG_PatternHeight:
      mVC5.patternHeight = val;
      break;
    case VC5_TAG_SubbandNumber:
      if (val >= numSubbands)
        ThrowRDE("Bad subband number %u", val);
      mVC5.iSubband = val;
      break;
    case VC5_TAG_Quantization:
      mVC5.quantization = static_cast<short16>(val);
      break;
    case VC5_TAG_ComponentsPerSample:
      mVC5.cps = val;
      break;
    case VC5_TAG_PrescaleShift:
      for (int iWavelet = 0; iWavelet < Channel::numTransforms; ++iWavelet)
        channels[mVC5.iChannel].transforms[iWavelet].prescale =
            (val >> (14 - 2 * iWavelet)) & 0x03;
      break;
    default: { // A chunk.
      unsigned int chunkSize = 0;
      if (tag & VC5_TAG_LARGE_CHUNK) {
        chunkSize =
            static_cast<unsigned int>(((tag & 0xff) << 16) | (val & 0xffff));
      } else if (tag & VC5_TAG_SMALL_CHUNK) {
        chunkSize = (val & 0xffff);
      }

      if ((tag & VC5_TAG_LargeCodeblock) == VC5_TAG_LargeCodeblock) {
        decodeLargeCodeblock(mBs.getStream(chunkSize, 4));
        break;
      }

      // And finally, we got here if we didn't handle this tag/maybe-chunk.

      // Magic, all the other 'large' chunks are actually optional,
      // and don't specify any chunk bytes-to-be-skipped.
      if (tag & VC5_TAG_LARGE_CHUNK) {
        optional = true;
        chunkSize = 0;
      }

      if (!optional)
        ThrowRDE("Unknown (unhandled) non-optional Tag 0x%04x", tag);

      if (chunkSize)
        mBs.skipBytes(chunkSize, 4);

      break;
    }
    }

    done = true;
    for (int iChannel = 0; iChannel < numChannels && done; ++iChannel) {
      Wavelet& wavelet = channels[iChannel].transforms[0].wavelet;
      if (!wavelet.isInitialized())
        done = false;
      if (!wavelet.allBandsValid())
        done = false;
    }
  }

  decodeFinalWavelet();
}

void VC5Decompressor::decodeLowPassBand(const ByteStream& bs,
                                        Array2DRef<int16_t> dst) {
  BitPumpMSB bits(bs);
  for (auto row = 0U; row < dst.height; ++row) {
    for (auto col = 0U; col < dst.width; ++col)
      dst(col, row) = static_cast<int16_t>(bits.getBits(mVC5.lowpassPrecision));
  }
}

void VC5Decompressor::decodeHighPassBand(const ByteStream& bs, int band,
                                         Wavelet* wavelet) {
  BitPumpMSB bits(bs);
  // decode highpass band
  int pixelValue = 0;
  unsigned int count = 0;
  int nPixels = wavelet->width * wavelet->height;
  for (int iPixel = 0; iPixel < nPixels;) {
    getRLV(&bits, &pixelValue, &count);
    for (; count > 0; --count) {
      if (iPixel > nPixels)
        ThrowRDE("Buffer overflow");
      wavelet->bands[band].data[iPixel] = static_cast<int16_t>(pixelValue);
      ++iPixel;
    }
  }
  if (bits.getPosition() < bits.getSize()) {
    getRLV(&bits, &pixelValue, &count);
    if (pixelValue != MARKER_BAND_END || count != 0)
      ThrowRDE("EndOfBand marker not found");
  }
  wavelet->bands[band].quant = mVC5.quantization;
}

void VC5Decompressor::decodeLargeCodeblock(const ByteStream& bs) {
  static constexpr std::array<int, numSubbands> subband_wavelet_index = {
      2, 2, 2, 2, 1, 1, 1, 0, 0, 0};
  static constexpr std::array<int, numSubbands> subband_band_index = {
      0, 1, 2, 3, 1, 2, 3, 1, 2, 3};
  const int idx = subband_wavelet_index[mVC5.iSubband];
  const int band = subband_band_index[mVC5.iSubband];
  uint16_t channelWidth = mVC5.imgWidth / mVC5.patternWidth;
  uint16_t channelHeight = mVC5.imgHeight / mVC5.patternHeight;

  if (mVC5.patternWidth != 2 || mVC5.patternHeight != 2)
    ThrowRDE("Invalid RAW file, pattern size != 2x2");

  auto& transforms = channels[mVC5.iChannel].transforms;

  // Initialize wavelets
  uint16_t waveletWidth = roundUpDivision(channelWidth, 2);
  uint16_t waveletHeight = roundUpDivision(channelHeight, 2);
  for (Transform& transform : transforms) {
    Wavelet& wavelet = transform.wavelet;
    if (wavelet.isInitialized()) {
      if (wavelet.width != waveletWidth || wavelet.height != waveletHeight)
        wavelet.clear();
    }
    if (!wavelet.isInitialized())
      wavelet.initialize(waveletWidth, waveletHeight);

    // Pad dimensions as necessary and divide them by two for the next wavelet
    for (auto* dimension : {&waveletWidth, &waveletHeight})
      *dimension = roundUpDivision(*dimension, 2);
  }

  Wavelet& wavelet = transforms[idx].wavelet;
  if (mVC5.iSubband == 0) {
    assert(band == 0);
    decodeLowPassBand(bs, wavelet.bandAsArray2DRef(0));
  } else {
    decodeHighPassBand(bs, band, &wavelet);
  }
  wavelet.setBandValid(band);

  // If this wavelet is fully decoded, reconstruct the low-pass band of
  // the next lower wavelet
  if (idx > 0 && wavelet.allBandsValid() &&
      !transforms[idx - 1].wavelet.isBandValid(0)) {
    auto& data = transforms[idx - 1].wavelet.bands[0].data;
    data.clear();
    data.shrink_to_fit();
    data = wavelet.reconstructLowband(transforms[idx].prescale);
    transforms[idx - 1].wavelet.setBandValid(0);
  }

  mVC5.iSubband++;
  if (mVC5.iSubband == numSubbands) {
    mVC5.iChannel++;
    mVC5.iSubband = 0;
  }
}

void VC5Decompressor::decodeFinalWavelet() {
  // Decode final wavelet into image
  Array2DRef<uint16_t> out(reinterpret_cast<uint16_t*>(mImg->getData()),
                           static_cast<unsigned int>(mImg->dim.x),
                           static_cast<unsigned int>(mImg->dim.y),
                           mImg->pitch / sizeof(uint16_t));

  unsigned int width = 2 * channels[0].transforms[0].wavelet.width;
  unsigned int height = 2 * channels[0].transforms[0].wavelet.height;

  std::array<std::vector<int16_t>, numChannels> lowbands_storage;
  std::array<Array2DRef<int16_t>, numChannels> lowbands;
  for (unsigned int iChannel = 0; iChannel < numChannels; ++iChannel) {
    auto& transform = channels[iChannel].transforms[0];
    assert(2 * transform.wavelet.width == width);
    assert(2 * transform.wavelet.height == height);
    lowbands_storage[iChannel] =
        transform.wavelet.reconstructLowband(transform.prescale, true);
    lowbands[iChannel] =
        Array2DRef<int16_t>(lowbands_storage[iChannel].data(), width, height);
  }

  // Convert to RGGB output
  // FIXME: this *should* be threadedable nicely.
  for (unsigned int row = 0; row < height; ++row) {
    for (unsigned int col = 0; col < width; ++col) {
      const int mid = 2048;

      int gs = lowbands[0](col, row);
      int rg = lowbands[1](col, row) - mid;
      int bg = lowbands[2](col, row) - mid;
      int gd = lowbands[3](col, row) - mid;

      int r = gs + 2 * rg;
      int b = gs + 2 * bg;
      int g1 = gs + gd;
      int g2 = gs - gd;

      out(2 * col + 0, 2 * row + 0) = static_cast<uint16_t>(mVC5LogTable[r]);
      out(2 * col + 1, 2 * row + 0) = static_cast<uint16_t>(mVC5LogTable[g1]);
      out(2 * col + 0, 2 * row + 1) = static_cast<uint16_t>(mVC5LogTable[g2]);
      out(2 * col + 1, 2 * row + 1) = static_cast<uint16_t>(mVC5LogTable[b]);
    }
  }
}

// static
void VC5Decompressor::getRLV(BitPumpMSB* bits, int* value,
                             unsigned int* count) {
  unsigned int iTab;

  // Ensure the maximum number of bits are cached to make peekBits() as fast as
  // possible.
  bits->fill(table17.entries[table17.length - 1].size);
  for (iTab = 0; iTab < table17.length; ++iTab) {
    if (table17.entries[iTab].bits ==
        bits->peekBits(table17.entries[iTab].size))
      break;
  }
  if (iTab >= table17.length)
    ThrowRDE("Code not found in codebook");

  bits->skipBits(table17.entries[iTab].size);
  *value = table17.entries[iTab].value;
  *count = table17.entries[iTab].count;
  if (*value != 0) {
    if (bits->getBits(1))
      *value = -(*value);
  }
}

} // namespace rawspeed
