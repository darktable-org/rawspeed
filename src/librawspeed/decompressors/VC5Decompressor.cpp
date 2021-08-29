/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2018 Stefan Löffler
    Copyright (C) 2018-2019 Roman Lebedev

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

#include "rawspeedconfig.h" // for HAVE_OPENMP
#include "decompressors/VC5Decompressor.h"
#include "common/Array2DRef.h"            // for Array2DRef
#include "common/Common.h"                // for clampBits, roundUpDivision
#include "common/Point.h"                 // for iPoint2D
#include "common/RawspeedException.h"     // for RawspeedException
#include "common/SimpleLUT.h"             // for SimpleLUT, SimpleLUT<>::va...
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "io/Endianness.h"                // for Endianness, Endianness::big
#include <cassert>                        // for assert
#include <cmath>                          // for pow
#include <initializer_list>               // for initializer_list
#include <limits>                         // for numeric_limits
#include <optional>                       // for optional
#include <string>                         // for string
#include <utility>                        // for move

namespace {

// Definitions needed by table17.inc
// Taken from
// https://github.com/gopro/gpr/blob/a513701afce7b03173213a2f67dfd9dd28fa1868/source/lib/vc5_decoder/vlc.h
struct RLV {
  uint_fast8_t size; //!< Size of code word in bits
  uint32_t bits;     //!< Code word bits right justified
  uint16_t count;    //!< Run length
  uint16_t value;    //!< Run value (unsigned)
};
#define RLVTABLE(n)                                                            \
  struct {                                                                     \
    const uint32_t length;                                                     \
    const RLV entries[n];                                                      \
  } constexpr
#include "gopro/vc5/table17.inc"

constexpr int16_t decompand(int16_t val) {
  double c = val;
  // Invert companding curve
  c += (c * c * c * 768) / (255. * 255. * 255.);
  if (c > std::numeric_limits<int16_t>::max())
    return std::numeric_limits<int16_t>::max();
  if (c < std::numeric_limits<int16_t>::min())
    return std::numeric_limits<int16_t>::min();
  return c;
}

#ifndef NDEBUG
[[maybe_unused]] const int ignore = []() {
  for (const RLV& entry : table17.entries) {
    assert(((-decompand(entry.value)) == decompand(-int16_t(entry.value))) &&
           "negation of decompanded value is the same as decompanding of "
           "negated value");
  }
  return 0;
}();
#endif

const std::array<RLV, table17.length> decompandedTable17 = []() {
  std::array<RLV, table17.length> d;
  for (auto i = 0U; i < table17.length; i++) {
    d[i] = table17.entries[i];
    d[i].value = decompand(table17.entries[i].value);
  }
  return d;
}();

} // namespace

#define PRECISION_MIN 8
#define PRECISION_MAX 16

#define MARKER_BAND_END 1

namespace rawspeed {

void VC5Decompressor::Wavelet::setBandValid(const int band) {
  mDecodedBandMask |= (1 << band);
}

bool VC5Decompressor::Wavelet::isBandValid(const int band) const {
  return mDecodedBandMask & (1 << band);
}

bool VC5Decompressor::Wavelet::allBandsValid() const {
  return mDecodedBandMask == static_cast<uint32_t>((1 << maxBands) - 1);
}

Array2DRef<const int16_t>
VC5Decompressor::Wavelet::bandAsArray2DRef(const unsigned int iBand) const {
  return bands[iBand]->data;
}

namespace {
const auto convolute = [](int row, int col, std::array<int, 4> muls,
                          const Array2DRef<const int16_t> high, auto lowGetter,
                          int DescaleShift = 0) {
  auto highCombined = muls[0] * high(row, col);
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

struct ConvolutionParams {
  struct First {
    static constexpr std::array<int, 4> mul_even = {+1, +11, -4, +1};
    static constexpr std::array<int, 4> mul_odd = {-1, +5, +4, -1};
    static constexpr int coord_shift = 0;
  };
  static constexpr First First{};

  struct Middle {
    static constexpr std::array<int, 4> mul_even = {+1, +1, +8, -1};
    static constexpr std::array<int, 4> mul_odd = {-1, -1, +8, +1};
    static constexpr int coord_shift = -1;
  };
  static constexpr Middle Middle{};

  struct Last {
    static constexpr std::array<int, 4> mul_even = {+1, -1, +4, +5};
    static constexpr std::array<int, 4> mul_odd = {-1, +1, -4, +11};
    static constexpr int coord_shift = -2;
  };
  static constexpr Last Last{};
};

} // namespace

void VC5Decompressor::Wavelet::reconstructPass(
    const Array2DRef<int16_t> dst, const Array2DRef<const int16_t> high,
    const Array2DRef<const int16_t> low) noexcept {
  auto process = [low, high, dst](auto segment, int row, int col) {
    auto lowGetter = [&row, &col, low](int delta) {
      return low(row + decltype(segment)::coord_shift + delta, col);
    };
    auto convolution = [&row, &col, high, lowGetter](std::array<int, 4> muls) {
      return convolute(row, col, muls, high, lowGetter, /*DescaleShift*/ 0);
    };

    int even = convolution(decltype(segment)::mul_even);
    int odd = convolution(decltype(segment)::mul_odd);

    dst(2 * row, col) = static_cast<int16_t>(even);
    dst(2 * row + 1, col) = static_cast<int16_t>(odd);
  };

  // Vertical reconstruction
#ifdef HAVE_OPENMP
#pragma omp for schedule(static)
#endif
  for (int row = 0; row < dst.height / 2; ++row) {
    if (row == 0) {
      // 1st row
      for (int col = 0; col < dst.width; ++col)
        process(ConvolutionParams::First, row, col);
    } else if (row + 1 < dst.height / 2) {
      // middle rows
      for (int col = 0; col < dst.width; ++col)
        process(ConvolutionParams::Middle, row, col);
    } else {
      // last row
      for (int col = 0; col < dst.width; ++col)
        process(ConvolutionParams::Last, row, col);
    }
  }
}

void VC5Decompressor::Wavelet::combineLowHighPass(
    const Array2DRef<int16_t> dst, const Array2DRef<const int16_t> low,
    const Array2DRef<const int16_t> high, int descaleShift,
    bool clampUint = false) noexcept {
  auto process = [low, high, descaleShift, clampUint, dst](auto segment,
                                                           int row, int col) {
    auto lowGetter = [&row, &col, low](int delta) {
      return low(row, col + decltype(segment)::coord_shift + delta);
    };
    auto convolution = [&row, &col, high, lowGetter,
                        descaleShift](std::array<int, 4> muls) {
      return convolute(row, col, muls, high, lowGetter, descaleShift);
    };

    int even = convolution(decltype(segment)::mul_even);
    int odd = convolution(decltype(segment)::mul_odd);

    if (clampUint) {
      even = clampBits(even, 14);
      odd = clampBits(odd, 14);
    }
    dst(row, 2 * col) = static_cast<int16_t>(even);
    dst(row, 2 * col + 1) = static_cast<int16_t>(odd);
  };

  // Horizontal reconstruction
#ifdef HAVE_OPENMP
#pragma omp for schedule(static)
#endif
  for (int row = 0; row < dst.height; ++row) {
    // First col
    int col = 0;
    process(ConvolutionParams::First, row, col);
    // middle cols
    for (col = 1; col + 1 < dst.width / 2; ++col) {
      process(ConvolutionParams::Middle, row, col);
    }
    // last col
    process(ConvolutionParams::Last, row, col);
  }
}

void VC5Decompressor::Wavelet::ReconstructableBand::processLow() noexcept {
#ifdef HAVE_OPENMP
#pragma omp single
#endif
  lowpass = Array2DRef<int16_t>::create(lowpass_storage, wavelet.width,
                                        2 * wavelet.height);

  const Array2DRef<const int16_t> highlow = wavelet.bandAsArray2DRef(2);
  const Array2DRef<const int16_t> lowlow = wavelet.bandAsArray2DRef(0);

  // Reconstruct the "immediates", the actual low pass ...
  Wavelet::reconstructPass(lowpass, highlow, lowlow);
}

void VC5Decompressor::Wavelet::ReconstructableBand::processHigh() noexcept {
#ifdef HAVE_OPENMP
#pragma omp single
#endif
  highpass = Array2DRef<int16_t>::create(highpass_storage, wavelet.width,
                                         2 * wavelet.height);

  const Array2DRef<const int16_t> highhigh = wavelet.bandAsArray2DRef(3);
  const Array2DRef<const int16_t> lowhigh = wavelet.bandAsArray2DRef(1);

  Wavelet::reconstructPass(highpass, highhigh, lowhigh);
}

void VC5Decompressor::Wavelet::ReconstructableBand::combine() noexcept {
  int16_t descaleShift = (wavelet.prescale == 2 ? 2 : 0);

#ifdef HAVE_OPENMP
#pragma omp single
#endif
  data = Array2DRef<int16_t>::create(data_storage, 2 * wavelet.width,
                                     2 * wavelet.height);

  // And finally, combine the low pass, and high pass.
  Wavelet::combineLowHighPass(data, lowpass, highpass, descaleShift, clampUint);
}

void VC5Decompressor::Wavelet::ReconstructableBand::decode() noexcept {
  assert(wavelet.allBandsValid());
  assert(data_storage.empty());
  processLow();
  processHigh();
  combine();
}

VC5Decompressor::VC5Decompressor(ByteStream bs, const RawImage& img)
    : mRaw(img), mBs(std::move(bs)) {
  if (!mRaw->dim.hasPositiveArea())
    ThrowRDE("Bad image dimensions.");

  if (mRaw->dim.x % mVC5.patternWidth != 0)
    ThrowRDE("Width %u is not a multiple of %u", mRaw->dim.x,
             mVC5.patternWidth);

  if (mRaw->dim.y % mVC5.patternHeight != 0)
    ThrowRDE("Height %u is not a multiple of %u", mRaw->dim.y,
             mVC5.patternHeight);

  // Initialize wavelet sizes.
  for (Channel& channel : channels) {
    uint16_t waveletWidth = mRaw->dim.x;
    uint16_t waveletHeight = mRaw->dim.y;
    for (Wavelet& wavelet : channel.wavelets) {
      // Pad dimensions as necessary and divide them by two for the next wavelet
      for (auto* dimension : {&waveletWidth, &waveletHeight})
        *dimension = roundUpDivision(*dimension, 2);
      wavelet.width = waveletWidth;
      wavelet.height = waveletHeight;

      wavelet.bands.resize(
          &wavelet == channel.wavelets.begin() ? 1 : Wavelet::maxBands);
    }
  }

  if (img->whitePoint <= 0 || img->whitePoint > int(((1U << 16U) - 1U)))
    ThrowRDE("Bad white level %i", img->whitePoint);

  outputBits = 0;
  for (int wp = img->whitePoint; wp != 0; wp >>= 1)
    ++outputBits;
  assert(outputBits <= 16);

  parseVC5();
}

void VC5Decompressor::initVC5LogTable() {
  mVC5LogTable = decltype(mVC5LogTable)(
      [outputBits = outputBits](unsigned i, unsigned tableSize) {
        // The vanilla "inverse log" curve for decoding.
        auto normalizedCurve = [](auto normalizedI) {
          return (std::pow(113.0, normalizedI) - 1) / 112.0;
        };

        auto normalizeI = [tableSize](auto x) { return x / (tableSize - 1.0); };
        auto denormalizeY = [maxVal = std::numeric_limits<uint16_t>::max()](
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

void VC5Decompressor::parseVC5() {
  mBs.setByteOrder(Endianness::big);

  assert(mRaw->dim.x > 0);
  assert(mRaw->dim.y > 0);

  // All VC-5 data must start with "VC-%" (0x56432d35)
  if (mBs.getU32() != 0x56432d35)
    ThrowRDE("not a valid VC-5 datablock");

  bool done = false;
  while (!done) {
    auto tag = static_cast<VC5Tag>(mBs.getU16());
    uint16_t val = mBs.getU16();

    bool optional = matches(tag, VC5Tag::Optional);
    if (optional)
      tag = -tag;

    switch (tag) {
    case VC5Tag::ChannelCount:
      if (val != numChannels)
        ThrowRDE("Bad channel count %u, expected %u", val, numChannels);
      break;
    case VC5Tag::ImageWidth:
      if (val != mRaw->dim.x)
        ThrowRDE("Image width mismatch: %u vs %u", val, mRaw->dim.x);
      break;
    case VC5Tag::ImageHeight:
      if (val != mRaw->dim.y)
        ThrowRDE("Image height mismatch: %u vs %u", val, mRaw->dim.y);
      break;
    case VC5Tag::LowpassPrecision:
      if (val < PRECISION_MIN || val > PRECISION_MAX)
        ThrowRDE("Invalid precision %i", val);
      mVC5.lowpassPrecision = val;
      break;
    case VC5Tag::ChannelNumber:
      if (val >= numChannels)
        ThrowRDE("Bad channel number (%u)", val);
      mVC5.iChannel = val;
      break;
    case VC5Tag::ImageFormat:
      if (val != mVC5.imgFormat)
        ThrowRDE("Image format %i is not 4(RAW)", val);
      break;
    case VC5Tag::SubbandCount:
      if (val != numSubbands)
        ThrowRDE("Unexpected subband count %u, expected %u", val, numSubbands);
      break;
    case VC5Tag::MaxBitsPerComponent:
      if (val != VC5_LOG_TABLE_BITWIDTH) {
        ThrowRDE("Bad bits per componend %u, not %u", val,
                 VC5_LOG_TABLE_BITWIDTH);
      }
      break;
    case VC5Tag::PatternWidth:
      if (val != mVC5.patternWidth)
        ThrowRDE("Bad pattern width %u, not %u", val, mVC5.patternWidth);
      break;
    case VC5Tag::PatternHeight:
      if (val != mVC5.patternHeight)
        ThrowRDE("Bad pattern height %u, not %u", val, mVC5.patternHeight);
      break;
    case VC5Tag::SubbandNumber:
      if (val >= numSubbands)
        ThrowRDE("Bad subband number %u", val);
      mVC5.iSubband = val;
      break;
    case VC5Tag::Quantization:
      mVC5.quantization = static_cast<int16_t>(val);
      break;
    case VC5Tag::ComponentsPerSample:
      if (val != mVC5.cps)
        ThrowRDE("Bad component per sample count %u, not %u", val, mVC5.cps);
      break;
    case VC5Tag::PrescaleShift:
      // FIXME: something is wrong. We get this before VC5Tag::ChannelNumber.
      // Defaulting to 'mVC5.iChannel=0' seems to work *for existing samples*.
      for (int iWavelet = 0; iWavelet < numWaveletLevels; ++iWavelet) {
        auto& channel = channels[mVC5.iChannel];
        auto& wavelet = channel.wavelets[1 + iWavelet];
        wavelet.prescale =
            extractHighBits(val, 2 * iWavelet, /*effectiveBitwidth=*/14) & 0x03;
      }
      break;
    default: { // A chunk.
      unsigned int chunkSize = 0;
      if (matches(tag, VC5Tag::LARGE_CHUNK)) {
        chunkSize = static_cast<unsigned int>(
            ((static_cast<std::underlying_type<VC5Tag>::type>(tag) & 0xff)
             << 16) |
            (val & 0xffff));
      } else if (matches(tag, VC5Tag::SMALL_CHUNK)) {
        chunkSize = (val & 0xffff);
      }

      if (is(tag, VC5Tag::LargeCodeblock)) {
        parseLargeCodeblock(mBs.getStream(chunkSize, 4));
        break;
      }

      // And finally, we got here if we didn't handle this tag/maybe-chunk.

      // Magic, all the other 'large' chunks are actually optional,
      // and don't specify any chunk bytes-to-be-skipped.
      if (matches(tag, VC5Tag::LARGE_CHUNK)) {
        optional = true;
        chunkSize = 0;
      }

      if (!optional) {
        ThrowRDE("Unknown (unhandled) non-optional Tag 0x%04hx",
                 static_cast<std::underlying_type<VC5Tag>::type>(tag));
      }

      if (chunkSize)
        mBs.skipBytes(chunkSize, 4);

      break;
    }
    }

    done = true;
    for (int iChannel = 0; iChannel < numChannels && done; ++iChannel) {
      Wavelet& wavelet = channels[iChannel].wavelets[0];
      if (!wavelet.isBandValid(0))
        done = false;
    }
  }
}

VC5Decompressor::Wavelet::LowPassBand::LowPassBand(const Wavelet& wavelet_,
                                                   ByteStream bs_,
                                                   uint16_t lowpassPrecision_)
    : AbstractDecodeableBand(wavelet_, std::move(bs_)),
      lowpassPrecision(lowpassPrecision_) {
  // Low-pass band is a uncompressed version of the image, hugely downscaled.
  // It consists of width * height pixels, `lowpassPrecision` each.
  // We can easily check that we have sufficient amount of bits to decode it.
  const auto waveletArea = iPoint2D(wavelet.width, wavelet.height).area();
  const auto bitsTotal = waveletArea * lowpassPrecision;
  const auto bytesTotal = roundUpDivision(bitsTotal, 8);
  bs = bs.getStream(bytesTotal); // And clamp the size while we are at it.
}

void VC5Decompressor::Wavelet::LowPassBand::decode() {
  data =
      Array2DRef<int16_t>::create(data_storage, wavelet.width, wavelet.height);

  BitPumpMSB bits(bs);
  for (auto row = 0; row < data.height; ++row) {
    for (auto col = 0; col < data.width; ++col)
      data(row, col) = static_cast<int16_t>(bits.getBits(lowpassPrecision));
  }
}

void VC5Decompressor::Wavelet::HighPassBand::decode() {
  class DeRLVer final {
    BitPumpMSB bits;
    const int16_t quant;

    int16_t pixelValue = 0;
    unsigned int numPixelsLeft = 0;

    void decodeNextPixelGroup() {
      assert(numPixelsLeft == 0);
      std::tie(pixelValue, numPixelsLeft) = getRLV(bits);
    }

  public:
    DeRLVer(const ByteStream& bs, int16_t quant_) : bits(bs), quant(quant_) {}

    void verifyIsAtEnd() {
      if (numPixelsLeft != 0)
        ThrowRDE("Not all pixels consumed?");
      decodeNextPixelGroup();
      static_assert(decompand(MARKER_BAND_END) == MARKER_BAND_END,
                    "passthrough");
      if (pixelValue != MARKER_BAND_END || numPixelsLeft != 0)
        ThrowRDE("EndOfBand marker not found");
    }

    int16_t decode() {
      auto dequantize = [quant = quant](int16_t val) -> int16_t {
        return val * quant;
      };

      if (numPixelsLeft == 0) {
        decodeNextPixelGroup();
        pixelValue = dequantize(pixelValue);
      }

      if (numPixelsLeft == 0)
        ThrowRDE("Got EndOfBand marker while looking for next pixel");

      --numPixelsLeft;
      return pixelValue;
    }
  };

  // decode highpass band
  DeRLVer d(bs, quant);
  data =
      Array2DRef<int16_t>::create(data_storage, wavelet.width, wavelet.height);
  for (int row = 0; row != wavelet.height; ++row)
    for (int col = 0; col != wavelet.width; ++col)
      data(row, col) = d.decode();
  d.verifyIsAtEnd();
}

void VC5Decompressor::parseLargeCodeblock(const ByteStream& bs) {
  static const auto subband_wavelet_index = []() {
    std::array<int, numSubbands> wavelets;
    int wavelet = 0;
    for (auto i = wavelets.size() - 1; i > 0;) {
      for (auto t = 0; t < numWaveletLevels; t++) {
        wavelets[i] = wavelet;
        i--;
      }
      if (i > 0)
        wavelet++;
    }
    wavelets.front() = wavelet;
    return wavelets;
  }();
  static const auto subband_band_index = []() {
    std::array<int, numSubbands> bands;
    bands.front() = 0;
    for (auto i = 1U; i < bands.size();) {
      for (int t = 1; t <= numWaveletLevels;) {
        bands[i] = t;
        t++;
        i++;
      }
    }
    return bands;
  }();

  if (!mVC5.iSubband.has_value())
    ThrowRDE("Did not see VC5Tag::SubbandNumber yet");

  const int idx = subband_wavelet_index[mVC5.iSubband.value()];
  const int band = subband_band_index[mVC5.iSubband.value()];

  auto& wavelets = channels[mVC5.iChannel].wavelets;

  Wavelet& wavelet = wavelets[1 + idx];
  if (wavelet.isBandValid(band)) {
    ThrowRDE("Band %u for wavelet %u on channel %u was already seen", band, idx,
             mVC5.iChannel);
  }

  std::unique_ptr<Wavelet::AbstractBand>& dstBand = wavelet.bands[band];
  if (mVC5.iSubband.value() == 0) {
    assert(band == 0);
    // low-pass band, only one, for the smallest wavelet, per channel per image
    if (!mVC5.lowpassPrecision.has_value())
      ThrowRDE("Did not see VC5Tag::LowpassPrecision yet");
    dstBand = std::make_unique<Wavelet::LowPassBand>(
        wavelet, bs, mVC5.lowpassPrecision.value());
    mVC5.lowpassPrecision.reset();
  } else {
    if (!mVC5.quantization.has_value())
      ThrowRDE("Did not see VC5Tag::Quantization yet");
    dstBand = std::make_unique<Wavelet::HighPassBand>(
        wavelet, bs, mVC5.quantization.value());
    mVC5.quantization.reset();
  }
  wavelet.setBandValid(band);

  // If this wavelet is fully specified, mark the low-pass band of the
  // next lower wavelet as specified.
  if (wavelet.allBandsValid()) {
    Wavelet& nextWavelet = wavelets[idx];
    assert(!nextWavelet.isBandValid(0));
    nextWavelet.bands[0] =
        std::make_unique<Wavelet::ReconstructableBand>(wavelet,
                                                       /*clampUint=*/idx == 0);
    nextWavelet.setBandValid(0);
  }

  mVC5.iSubband.reset();
}

void VC5Decompressor::prepareBandDecodingPlan() {
  assert(allDecodeableBands.empty());
  allDecodeableBands.reserve(numSubbandsTotal);
  // All the high-pass bands for all wavelets,
  // in this specific order of decreasing worksize.
  for (int waveletLevel = 0; waveletLevel < numWaveletLevels; waveletLevel++) {
    for (auto channelId = 0; channelId < numChannels; channelId++) {
      for (int bandId = 1; bandId <= numHighPassBands; bandId++) {
        auto& channel = channels[channelId];
        auto& wavelet = channel.wavelets[1 + waveletLevel];
        auto& band = *wavelet.bands[bandId];
        auto& decodeableHighPassBand =
            dynamic_cast<Wavelet::HighPassBand&>(band);
        allDecodeableBands.emplace_back(decodeableHighPassBand, wavelet);
      }
    }
  }
  // The low-pass bands at the end. I'm guessing they should be fast to
  // decode.
  for (Channel& channel : channels) {
    // Low-pass band of the smallest wavelet.
    Wavelet& smallestWavelet = channel.wavelets.back();
    auto& decodeableLowPassBand =
        dynamic_cast<Wavelet::LowPassBand&>(*smallestWavelet.bands[0]);
    allDecodeableBands.emplace_back(decodeableLowPassBand, smallestWavelet);
  }
  assert(allDecodeableBands.size() == numSubbandsTotal);
}

void VC5Decompressor::prepareBandReconstruction() {
  assert(reconstructionSteps.empty());
  reconstructionSteps.reserve(numLowPassBandsTotal);
  // For every channel, recursively reconstruct the low-pass bands.
  for (auto& channel : channels) {
    // Reconstruct the intermediate lowpass bands.
    for (int waveletLevel = channel.wavelets.size() - 1; waveletLevel > 0;
         waveletLevel--) {
      Wavelet& wavelet = channel.wavelets[waveletLevel];

      Wavelet& nextWavelet = channel.wavelets[waveletLevel - 1];
      auto& band =
          dynamic_cast<Wavelet::ReconstructableBand&>(*nextWavelet.bands[0]);

      reconstructionSteps.emplace_back(wavelet, band);
    }
  }
  assert(reconstructionSteps.size() == numLowPassBandsTotal);
}

void VC5Decompressor::prepareDecodingPlan() {
  prepareBandDecodingPlan();
  prepareBandReconstruction();
}

void VC5Decompressor::decodeThread(bool* exceptionThrown) const noexcept {
  // Decode all the existing bands. May fail.
  decodeBands(exceptionThrown);

  // Proceed only if decoding did not fail.
  if (*exceptionThrown)
    return;

  // And now, reconstruct the low-pass bands.
  reconstructLowpassBands();

  // And finally!
  combineFinalLowpassBands();
}

void VC5Decompressor::decode(unsigned int offsetX, unsigned int offsetY,
                             unsigned int width, unsigned int height) {
  if (offsetX || offsetY || mRaw->dim != iPoint2D(width, height))
    ThrowRDE("VC5Decompressor expects to fill the whole image, not some tile.");

  initVC5LogTable();

  prepareDecodingPlan();

  bool exceptionThrown = false;
#ifdef HAVE_OPENMP
#pragma omp parallel default(none) shared(exceptionThrown)                     \
    num_threads(rawspeed_get_number_of_processor_cores())
#endif
  decodeThread(&exceptionThrown);

  std::string firstErr;
  if (mRaw->isTooManyErrors(1, &firstErr)) {
    assert(exceptionThrown);
    ThrowRDE("Too many errors encountered. Giving up. First Error:\n%s",
             firstErr.c_str());
  } else {
    assert(!exceptionThrown);
  }
}

void VC5Decompressor::decodeBands(bool* exceptionThrown) const noexcept {
#ifdef HAVE_OPENMP
#pragma omp for schedule(dynamic, 1)
#endif
  for (auto decodeableBand = allDecodeableBands.begin();
       decodeableBand < allDecodeableBands.end(); ++decodeableBand) {
    try {
      decodeableBand->band.decode();
    } catch (RawspeedException& err) {
      // Propagate the exception out of OpenMP magic.
      mRaw->setError(err.what());
#ifdef HAVE_OPENMP
#pragma omp atomic write
#endif
      *exceptionThrown = true;
#ifdef HAVE_OPENMP
#pragma omp cancel for
#endif
    }
  }
}

void VC5Decompressor::reconstructLowpassBands() const noexcept {
  for (const ReconstructionStep& step : reconstructionSteps) {
    step.band.decode();

#ifdef HAVE_OPENMP
#pragma omp single nowait
#endif
    step.wavelet.clear(); // we no longer need it.
  }
}

void VC5Decompressor::combineFinalLowpassBands() const noexcept {
  const Array2DRef<uint16_t> out(mRaw->getU16DataAsUncroppedArray2DRef());

  const int width = out.width / 2;
  const int height = out.height / 2;

  const Array2DRef<const int16_t> lowbands0 =
      channels[0].wavelets[0].bands[0]->data;
  const Array2DRef<const int16_t> lowbands1 =
      channels[1].wavelets[0].bands[0]->data;
  const Array2DRef<const int16_t> lowbands2 =
      channels[2].wavelets[0].bands[0]->data;
  const Array2DRef<const int16_t> lowbands3 =
      channels[3].wavelets[0].bands[0]->data;

  // Convert to RGGB output
#ifdef HAVE_OPENMP
#pragma omp for schedule(static)
#endif
  for (int row = 0; row < height; ++row) {
    for (int col = 0; col < width; ++col) {
      const int mid = 2048;

      int gs = lowbands0(row, col);
      int rg = lowbands1(row, col) - mid;
      int bg = lowbands2(row, col) - mid;
      int gd = lowbands3(row, col) - mid;

      int r = gs + 2 * rg;
      int b = gs + 2 * bg;
      int g1 = gs + gd;
      int g2 = gs - gd;

      out(2 * row + 0, 2 * col + 0) = static_cast<uint16_t>(mVC5LogTable[r]);
      out(2 * row + 0, 2 * col + 1) = static_cast<uint16_t>(mVC5LogTable[g1]);
      out(2 * row + 1, 2 * col + 0) = static_cast<uint16_t>(mVC5LogTable[g2]);
      out(2 * row + 1, 2 * col + 1) = static_cast<uint16_t>(mVC5LogTable[b]);
    }
  }
}

inline std::pair<int16_t /*value*/, unsigned int /*count*/>
VC5Decompressor::getRLV(BitPumpMSB& bits) {
  unsigned int iTab;

  static constexpr auto maxBits = 1 + table17.entries[table17.length - 1].size;

  // Ensure the maximum number of bits are cached to make peekBits() as fast as
  // possible.
  bits.fill(maxBits);
  for (iTab = 0; iTab < table17.length; ++iTab) {
    if (decompandedTable17[iTab].bits ==
        bits.peekBitsNoFill(decompandedTable17[iTab].size))
      break;
  }
  if (iTab >= table17.length)
    ThrowRDE("Code not found in codebook");

  bits.skipBitsNoFill(decompandedTable17[iTab].size);
  int16_t value = decompandedTable17[iTab].value;
  unsigned int count = decompandedTable17[iTab].count;
  if (value != 0) {
    if (bits.getBitsNoFill(1))
      value = -(value);
  }

  return {value, count};
}

} // namespace rawspeed
