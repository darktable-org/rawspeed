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

#include "rawspeedconfig.h"
#include "decompressors/VC5Decompressor.h"
#include "adt/Array1DRef.h"
#include "adt/Array2DRef.h"
#include "adt/Bit.h"
#include "adt/Casts.h"
#include "adt/Invariant.h"
#include "adt/Optional.h"
#include "adt/Point.h"
#include "bitstreams/BitStreamerMSB.h"
#include "codes/AbstractPrefixCode.h"
#include "codes/PrefixCode.h"
#include "common/BayerPhase.h"
#include "common/Common.h"
#include "common/ErrorLog.h"
#include "common/RawImage.h"
#include "common/SimpleLUT.h"
#include "decoders/RawDecoderException.h"
#include "io/Buffer.h"
#include "io/ByteStream.h"
#include "io/Endianness.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

// Definitions needed by table17.inc
// Taken from
// https://github.com/gopro/gpr/blob/a513701afce7b03173213a2f67dfd9dd28fa1868/source/lib/vc5_decoder/vlc.h
struct RLV final {
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

constexpr auto decompand(int16_t val) {
  double c = val;
  // Invert companding curve
  c += (c * c * c * 768) / (255. * 255. * 255.);
  if (c > std::numeric_limits<int16_t>::max())
    return std::numeric_limits<int16_t>::max();
  if (c < std::numeric_limits<int16_t>::min())
    return std::numeric_limits<int16_t>::min();
  return rawspeed::implicit_cast<int16_t>(c);
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

inline bool readValue(const bool& storage) {
  bool value;

#ifdef HAVE_OPENMP
#pragma omp atomic read
#endif
  value = storage;

  return value;
}

constexpr int PRECISION_MIN = 8;
constexpr int PRECISION_MAX = 16;
constexpr int MARKER_BAND_END = 1;

} // namespace

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

namespace {
template <typename LowGetter>
inline auto convolute(int row, int col, std::array<int, 4> muls,
                      const Array2DRef<const int16_t> high, LowGetter lowGetter,
                      int DescaleShift = 0) {
  auto highCombined = muls[0] * high(row, col);
  auto lowsCombined = [muls, &lowGetter]() {
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
  // NOTE: left shift of negative value is UB until C++20.
  total *= 1 << DescaleShift;
  // And average it.
  total >>= 1;
  return total;
}

struct ConvolutionParams final {
  struct First final {
    static constexpr std::array<int, 4> mul_even = {+1, +11, -4, +1};
    static constexpr std::array<int, 4> mul_odd = {-1, +5, +4, -1};
    static constexpr int coord_shift = 0;
  };

  struct Middle final {
    static constexpr std::array<int, 4> mul_even = {+1, +1, +8, -1};
    static constexpr std::array<int, 4> mul_odd = {-1, -1, +8, +1};
    static constexpr int coord_shift = -1;
  };

  struct Last final {
    static constexpr std::array<int, 4> mul_even = {+1, -1, +4, +5};
    static constexpr std::array<int, 4> mul_odd = {-1, +1, -4, +11};
    static constexpr int coord_shift = -2;
  };
};

} // namespace

VC5Decompressor::BandData VC5Decompressor::Wavelet::reconstructPass(
    const Array2DRef<const int16_t> high,
    const Array2DRef<const int16_t> low) noexcept {
  BandData combined(high.width(), 2 * high.height());
  const auto& dst = combined.description;

  auto process = [low, high, dst]<typename SegmentTy>(int row, int col) {
    auto lowGetter = [&row, &col, low](int delta) {
      return low(row + SegmentTy::coord_shift + delta, col);
    };
    auto convolution = [&row, &col, high, &lowGetter](std::array<int, 4> muls) {
      return convolute(row, col, muls, high, lowGetter, /*DescaleShift*/ 0);
    };

    int even = convolution(SegmentTy::mul_even);
    int odd = convolution(SegmentTy::mul_odd);

    dst(2 * row, col) = static_cast<int16_t>(even);
    dst(2 * row + 1, col) = static_cast<int16_t>(odd);
  };

#pragma GCC diagnostic push
// See https://bugs.llvm.org/show_bug.cgi?id=51666
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wshorten-64-to-32"
#ifdef HAVE_OPENMP
#pragma omp taskloop default(none) firstprivate(dst, process)                  \
    num_tasks(roundUpDivisionSafe(rawspeed_get_number_of_processor_cores(),    \
                                      numChannels))
#endif
  for (int row = 0; row < dst.height() / 2; ++row) {
#pragma GCC diagnostic pop
    if (row == 0) {
      // 1st row
      for (int col = 0; col < dst.width(); ++col)
        process.template operator()<ConvolutionParams::First>(row, col);
    } else if (row + 1 < dst.height() / 2) {
      // middle rows
      for (int col = 0; col < dst.width(); ++col)
        process.template operator()<ConvolutionParams::Middle>(row, col);
    } else {
      // last row
      for (int col = 0; col < dst.width(); ++col)
        process.template operator()<ConvolutionParams::Last>(row, col);
    }
  }

  return combined;
}

VC5Decompressor::BandData VC5Decompressor::Wavelet::combineLowHighPass(
    const Array2DRef<const int16_t> low, const Array2DRef<const int16_t> high,
    int descaleShift, bool clampUint = false,
    [[maybe_unused]] bool finalWavelet = false) noexcept {
  BandData combined(2 * high.width(), high.height());
  const auto& dst = combined.description;

  auto process = [low, high, descaleShift, clampUint,
                  dst]<typename SegmentTy>(int row, int col) {
    auto lowGetter = [&row, &col, low](int delta) {
      return low(row, col + SegmentTy::coord_shift + delta);
    };
    auto convolution = [&row, &col, high, &lowGetter,
                        descaleShift](std::array<int, 4> muls) {
      return convolute(row, col, muls, high, lowGetter, descaleShift);
    };

    int even = convolution(SegmentTy::mul_even);
    int odd = convolution(SegmentTy::mul_odd);

    if (clampUint) {
      even = clampBits(even, 14);
      odd = clampBits(odd, 14);
    }
    dst(row, 2 * col) = static_cast<int16_t>(even);
    dst(row, 2 * col + 1) = static_cast<int16_t>(odd);
  };

  // Horizontal reconstruction
#pragma GCC diagnostic push
  // See https://bugs.llvm.org/show_bug.cgi?id=51666
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wshorten-64-to-32"
#ifdef HAVE_OPENMP
#pragma omp taskloop if (finalWavelet) default(none)                           \
    firstprivate(dst, process) num_tasks(roundUpDivisionSafe(                  \
            rawspeed_get_number_of_processor_cores(), 2)) mergeable
#endif
  for (int row = 0; row < dst.height(); ++row) {
#pragma GCC diagnostic pop
    // First col
    int col = 0;
    process.template operator()<ConvolutionParams::First>(row, col);
    // middle cols
    for (col = 1; col + 1 < dst.width() / 2; ++col) {
      process.template operator()<ConvolutionParams::Middle>(row, col);
    }
    // last col
    process.template operator()<ConvolutionParams::Last>(row, col);
  }

  return combined;
}

void VC5Decompressor::Wavelet::ReconstructableBand::
    createLowpassReconstructionTask(const bool& exceptionThrown) noexcept {
  const auto& highlow = wavelet.bands[2]->data;
  const auto& lowlow = wavelet.bands[0]->data;
  auto& lowpass = intermediates.lowpass;

#ifdef HAVE_OPENMP
#pragma omp task default(none)                                                 \
    shared(exceptionThrown, highlow, lowlow, lowpass)                          \
    depend(in : highlow, lowlow) depend(out : lowpass)
#endif
  {
    // Proceed only if decoding did not fail.
    if (!readValue(exceptionThrown)) {
      assert(highlow.has_value() && lowlow.has_value() &&
             "Failed to produce precursor bands?");
      // Reconstruct the "immediates", the actual low pass ...
      assert(!lowpass.has_value() && "Combined this precursor band already?");
      lowpass.emplace(
          Wavelet::reconstructPass(highlow->description, lowlow->description));
    }
  }
}

void VC5Decompressor::Wavelet::ReconstructableBand::
    createHighpassReconstructionTask(const bool& exceptionThrown) noexcept {
  const auto& highhigh = wavelet.bands[3]->data;
  const auto& lowhigh = wavelet.bands[1]->data;
  auto& highpass = intermediates.highpass;

#ifdef HAVE_OPENMP
#pragma omp task default(none)                                                 \
    shared(exceptionThrown, highhigh, lowhigh, highpass)                       \
    depend(in : highhigh, lowhigh) depend(out : highpass)
#endif
  {
    // Proceed only if decoding did not fail.
    if (!readValue(exceptionThrown)) {
      assert(highhigh.has_value() && lowhigh.has_value() &&
             "Failed to produce precursor bands?");
      assert(!highpass.has_value() && "Combined this precursor band already?");
      highpass.emplace(Wavelet::reconstructPass(highhigh->description,
                                                lowhigh->description));
    }
  }
}

void VC5Decompressor::Wavelet::ReconstructableBand::
    createLowHighPassCombiningTask(const bool& exceptionThrown) noexcept {
  const auto& lowpass = intermediates.lowpass;
  const auto& highpass = intermediates.highpass;
  auto& reconstructedLowpass = data;

#ifdef HAVE_OPENMP
#pragma omp task default(none) shared(exceptionThrown)                         \
    depend(in : lowpass, highpass)
#endif
  {
    if (!readValue(exceptionThrown)) {
      wavelet.bands.clear();
    }
  }

#ifdef HAVE_OPENMP
#pragma omp task default(none)                                                 \
    shared(exceptionThrown, lowpass, highpass, reconstructedLowpass)           \
    depend(in : lowpass, highpass) depend(out : reconstructedLowpass)
#endif
  {
    // Proceed only if decoding did not fail.
    if (!readValue(exceptionThrown)) {
      assert(lowpass.has_value() && highpass.has_value() &&
             "Failed to combine precursor bands?");
      assert(!data.has_value() && "Reconstructed this band already?");

      int16_t descaleShift = (wavelet.prescale == 2 ? 2 : 0);

      // And finally, combine the low pass, and high pass.
      reconstructedLowpass.emplace(Wavelet::combineLowHighPass(
          lowpass->description, highpass->description, descaleShift, clampUint,
          finalWavelet));
    }
  }
}

void VC5Decompressor::Wavelet::ReconstructableBand::createDecodingTasks(
    ErrorLog& errLog, bool& exceptionThrow) noexcept {
  assert(wavelet.allBandsValid());
  createLowpassReconstructionTask(exceptionThrow);
  createHighpassReconstructionTask(exceptionThrow);
  createLowHighPassCombiningTask(exceptionThrow);
}

VC5Decompressor::VC5Decompressor(ByteStream bs, const RawImage& img)
    : mRaw(img), mBs(bs) {
  if (mRaw->getCpp() != 1 || mRaw->getDataType() != RawImageType::UINT16 ||
      mRaw->getBpp() != sizeof(uint16_t))
    ThrowRDE("Unexpected component count / data type");

  if (!mRaw->dim.hasPositiveArea())
    ThrowRDE("Bad image dimensions.");

  if (mRaw->dim.x % mVC5.patternWidth != 0)
    ThrowRDE("Width %u is not a multiple of %u", mRaw->dim.x,
             mVC5.patternWidth);

  if (mRaw->dim.y % mVC5.patternHeight != 0)
    ThrowRDE("Height %u is not a multiple of %u", mRaw->dim.y,
             mVC5.patternHeight);

  Optional<BayerPhase> p = getAsBayerPhase(mRaw->cfa);
  if (!p)
    ThrowRDE("Image has invalid CFA.");
  phase = *p;
  if (phase != BayerPhase::RGGB && phase != BayerPhase::GBRG)
    ThrowRDE("Unexpected bayer phase, please file a bug.");

  // Initialize wavelet sizes.
  for (Channel& channel : channels) {
    auto waveletWidth = implicit_cast<uint16_t>(mRaw->dim.x);
    auto waveletHeight = implicit_cast<uint16_t>(mRaw->dim.y);
    for (Wavelet& wavelet : channel.wavelets) {
      // Pad dimensions as necessary and divide them by two for the next wavelet
      for (auto* dimension : {&waveletWidth, &waveletHeight})
        *dimension =
            implicit_cast<uint16_t>(roundUpDivisionSafe(*dimension, 2));
      wavelet.width = waveletWidth;
      wavelet.height = waveletHeight;

      wavelet.bands.resize(
          &wavelet == channel.wavelets.begin() ? 1 : Wavelet::maxBands);
    }
  }

  if (*img->whitePoint <= 0 || *img->whitePoint > int(((1U << 16U) - 1U)))
    ThrowRDE("Bad white level %i", *img->whitePoint);

  outputBits = 0;
  for (int wp = *img->whitePoint; wp != 0; wp >>= 1)
    ++outputBits;
  invariant(outputBits <= 16);

  parseVC5();
}

static constexpr int DecompandedCodeValueBitWidth = 10;
static constexpr int RLVRunLengthBitWidth = 9;

void VC5Decompressor::initPrefixCodeDecoder() {
  using CodeSymbol = AbstractPrefixCode<VC5CodeTag>::CodeSymbol;
  using CodeValueTy = CodeTraits<VC5CodeTag>::CodeValueTy;

  std::vector<CodeSymbol> symbols;
  std::vector<CodeValueTy> codeValues;

  symbols.reserve(table17.length);
  for (const RLV& e : table17.entries)
    symbols.emplace_back(e.bits, e.size);

  codeValues.reserve(table17.length);
  for (const RLV& e : table17.entries) {
    CodeValueTy value = decompand(e.value);
    assert(rawspeed::isIntN(value, DecompandedCodeValueBitWidth));
    (void)DecompandedCodeValueBitWidth;
    assert(rawspeed::isIntN(e.count, RLVRunLengthBitWidth));
    value <<= RLVRunLengthBitWidth;
    value |= e.count;
    codeValues.emplace_back(value);
  }

  PrefixCode<VC5CodeTag> code(std::move(symbols), std::move(codeValues));
  codeDecoder.emplace(std::move(code));
  codeDecoder->setup(/*fullDecode_=*/false, /*fixDNGBug16_=*/false);
}

void VC5Decompressor::initVC5LogTable() {
  mVC5LogTable = decltype(mVC5LogTable)(
      [outputBits_ = outputBits](size_t i, unsigned tableSize) {
        // The vanilla "inverse log" curve for decoding.
        auto normalizedCurve = [](auto normalizedI) {
          return (std::pow(113.0, normalizedI) - 1) / 112.0;
        };

        auto normalizeI = [tableSize](auto x) {
          return implicit_cast<double>(x) / (tableSize - 1.0);
        };
        auto denormalizeY = [maxVal = std::numeric_limits<uint16_t>::max()](
                                auto y) { return maxVal * y; };
        // Adjust for output whitelevel bitdepth.
        auto rescaleY = [outputBits_](auto y) {
          auto scale = 16 - outputBits_;
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

  invariant(mRaw->dim.x > 0);
  invariant(mRaw->dim.y > 0);

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
      using enum VC5Tag;
    case ChannelCount:
      if (val != numChannels)
        ThrowRDE("Bad channel count %u, expected %u", val, numChannels);
      break;
    case ImageWidth:
      if (val != mRaw->dim.x)
        ThrowRDE("Image width mismatch: %u vs %u", val, mRaw->dim.x);
      break;
    case ImageHeight:
      if (val != mRaw->dim.y)
        ThrowRDE("Image height mismatch: %u vs %u", val, mRaw->dim.y);
      break;
    case LowpassPrecision:
      if (val < PRECISION_MIN || val > PRECISION_MAX)
        ThrowRDE("Invalid precision %i", val);
      mVC5.lowpassPrecision = val;
      break;
    case ChannelNumber:
      if (val >= numChannels)
        ThrowRDE("Bad channel number (%u)", val);
      mVC5.iChannel = val;
      break;
    case ImageFormat:
      if (val != mVC5.imgFormat)
        ThrowRDE("Image format %i is not 4(RAW)", val);
      break;
    case SubbandCount:
      if (val != numSubbands)
        ThrowRDE("Unexpected subband count %u, expected %u", val, numSubbands);
      break;
    case MaxBitsPerComponent:
      if (val != VC5_LOG_TABLE_BITWIDTH) {
        ThrowRDE("Bad bits per componend %u, not %u", val,
                 VC5_LOG_TABLE_BITWIDTH);
      }
      break;
    case PatternWidth:
      if (val != mVC5.patternWidth)
        ThrowRDE("Bad pattern width %u, not %u", val, mVC5.patternWidth);
      break;
    case PatternHeight:
      if (val != mVC5.patternHeight)
        ThrowRDE("Bad pattern height %u, not %u", val, mVC5.patternHeight);
      break;
    case SubbandNumber:
      if (val >= numSubbands)
        ThrowRDE("Bad subband number %u", val);
      mVC5.iSubband = val;
      break;
    case Quantization:
      mVC5.quantization = static_cast<int16_t>(val);
      break;
    case ComponentsPerSample:
      if (val != mVC5.cps)
        ThrowRDE("Bad component per sample count %u, not %u", val, mVC5.cps);
      break;
    case PrescaleShift:
      // FIXME: something is wrong. We get this before ChannelNumber.
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
      if (matches(tag, LARGE_CHUNK)) {
        chunkSize = static_cast<unsigned int>(
            ((static_cast<std::underlying_type_t<VC5Tag>>(tag) & 0xff) << 16) |
            (val & 0xffff));
      } else if (matches(tag, SMALL_CHUNK)) {
        chunkSize = (val & 0xffff);
      }

      if (is(tag, LargeCodeblock)) {
        parseLargeCodeblock(mBs.getStream(chunkSize, 4));
        break;
      }

      // And finally, we got here if we didn't handle this tag/maybe-chunk.

      // Magic, all the other 'large' chunks are actually optional,
      // and don't specify any chunk bytes-to-be-skipped.
      if (matches(tag, LARGE_CHUNK)) {
        optional = true;
        chunkSize = 0;
      }

      if (!optional) {
        ThrowRDE("Unknown (unhandled) non-optional Tag 0x%04hx",
                 static_cast<std::underlying_type_t<VC5Tag>>(tag));
      }

      if (chunkSize)
        mBs.skipBytes(chunkSize, 4);

      break;
    }
    }

    done = std::all_of(channels.begin(), channels.end(),
                       [](const Channel& channel) {
                         return channel.wavelets[0].isBandValid(0);
                       });
  }
}

void VC5Decompressor::Wavelet::AbstractDecodeableBand::createDecodingTasks(
    ErrorLog& errLog, bool& exceptionThrown) noexcept {
  auto& decodedData = data;

#ifdef HAVE_OPENMP
#pragma omp task default(none) shared(decodedData, errLog, exceptionThrown)    \
    depend(out : decodedData)
#endif
  {
    // Proceed only if decoding did not fail.
    if (!readValue(exceptionThrown)) {
      try {
        assert(!decodedData.has_value() && "Decoded this band already?");
        decodedData = decode();
      } catch (const RawspeedException& err) {
        // Propagate the exception out of OpenMP magic.
        errLog.setError(err.what());
#ifdef HAVE_OPENMP
#pragma omp atomic write
#endif
        exceptionThrown = true;
      } catch (...) {
        // We should not get any other exception type here.
        __builtin_unreachable();
      }
    }
  }
}

VC5Decompressor::Wavelet::LowPassBand::LowPassBand(Wavelet& wavelet_,
                                                   ByteStream bs,
                                                   uint16_t lowpassPrecision_)
    : AbstractDecodeableBand(wavelet_, bs.getAsArray1DRef()),
      lowpassPrecision(lowpassPrecision_) {
  // Low-pass band is a uncompressed version of the image, hugely downscaled.
  // It consists of width * height pixels, `lowpassPrecision` each.
  // We can easily check that we have sufficient amount of bits to decode it.
  const auto waveletArea = iPoint2D(wavelet.width, wavelet.height).area();
  const auto bitsTotal = waveletArea * lowpassPrecision;
  constexpr int bytesPerChunk = 8; // FIXME: or is it 4?
  constexpr int bitsPerChunk = 8 * bytesPerChunk;
  const auto chunksTotal = roundUpDivisionSafe(bitsTotal, bitsPerChunk);
  const auto bytesTotal = bytesPerChunk * chunksTotal;
  // And clamp the size / verify sufficient input while we are at it.
  // NOTE: this might fail (and should throw, not assert).
  input = bs.getStream(implicit_cast<Buffer::size_type>(bytesTotal))
              .getAsArray1DRef();
}

VC5Decompressor::BandData
VC5Decompressor::Wavelet::LowPassBand::decode() const noexcept {
  BandData lowpass(wavelet.width, wavelet.height);
  const auto& band = lowpass.description;

  BitStreamerMSB bits(input);
  for (auto row = 0; row < band.height(); ++row) {
    for (auto col = 0; col < band.width(); ++col)
      band(row, col) = static_cast<int16_t>(bits.getBits(lowpassPrecision));
  }

  return lowpass;
}

VC5Decompressor::BandData
VC5Decompressor::Wavelet::HighPassBand::decode() const {
  class DeRLVer final {
    const PrefixCodeDecoder& decoder;
    BitStreamerMSB bits;
    const int16_t quant;

    int16_t pixelValue = 0;
    unsigned int numPixelsLeft = 0;

    void decodeNextPixelGroup() {
      invariant(numPixelsLeft == 0);
      std::tie(pixelValue, numPixelsLeft) = getRLV(decoder, bits);
    }

  public:
    DeRLVer(const PrefixCodeDecoder& decoder_, Array1DRef<const uint8_t> input,
            int16_t quant_)
        : decoder(decoder_), bits(input), quant(quant_) {}

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
      auto dequantize = [quant_ = quant](int16_t val) {
        if (__builtin_mul_overflow(val, quant_, &val))
          ThrowRDE("Impossible RLV value given current quantum");
        return val;
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
  DeRLVer d(*decoder, input, quant);
  BandData highpass(wavelet.width, wavelet.height);
  const auto& band = highpass.description;
  for (int row = 0; row != wavelet.height; ++row)
    for (int col = 0; col != wavelet.width; ++col)
      band(row, col) = d.decode();
  d.verifyIsAtEnd();
  return highpass;
}

void VC5Decompressor::parseLargeCodeblock(ByteStream bs) {
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

  const int idx = subband_wavelet_index[*mVC5.iSubband];
  const int band = subband_band_index[*mVC5.iSubband];

  auto& wavelets = channels[mVC5.iChannel].wavelets;

  Wavelet& wavelet = wavelets[1 + idx];
  if (wavelet.isBandValid(band)) {
    ThrowRDE("Band %u for wavelet %u on channel %u was already seen", band, idx,
             mVC5.iChannel);
  }

  std::unique_ptr<Wavelet::AbstractBand>& dstBand = wavelet.bands[band];
  if (mVC5.iSubband == 0) {
    assert(band == 0);
    // low-pass band, only one, for the smallest wavelet, per channel per image
    if (!mVC5.lowpassPrecision.has_value())
      ThrowRDE("Did not see VC5Tag::LowpassPrecision yet");
    dstBand = std::make_unique<Wavelet::LowPassBand>(wavelet, bs,
                                                     *mVC5.lowpassPrecision);
    mVC5.lowpassPrecision.reset();
  } else {
    if (!mVC5.quantization.has_value())
      ThrowRDE("Did not see VC5Tag::Quantization yet");
    dstBand = std::make_unique<Wavelet::HighPassBand>(
        wavelet, bs.getAsArray1DRef(), codeDecoder, *mVC5.quantization);
    mVC5.quantization.reset();
  }
  wavelet.setBandValid(band);

  // If this wavelet is fully specified, mark the low-pass band of the
  // next lower wavelet as specified.
  if (wavelet.allBandsValid()) {
    Wavelet& nextWavelet = wavelets[idx];
    assert(!nextWavelet.isBandValid(0));
    bool finalWavelet = idx == 0;
    nextWavelet.bands[0] = std::make_unique<Wavelet::ReconstructableBand>(
        wavelet, /*clampUint=*/finalWavelet, finalWavelet);
    nextWavelet.setBandValid(0);
  }

  mVC5.iSubband.reset();
}

void VC5Decompressor::createWaveletBandDecodingTasks(
    bool& exceptionThrown) const noexcept {
  for (int waveletLevel = numWaveletLevels; waveletLevel >= 0; waveletLevel--) {
    const int numBandsInCurrentWavelet =
        waveletLevel == 0 ? 1 : Wavelet::maxBands;
    for (int bandId = numBandsInCurrentWavelet - 1; bandId >= 0; --bandId) {
      for (const auto& channel : channels) {
        channel.wavelets[waveletLevel].bands[bandId]->createDecodingTasks(
            static_cast<ErrorLog&>(*mRaw), exceptionThrown);
        if (readValue(exceptionThrown)) {
          return;
        }
      }
    }
  }
}

void VC5Decompressor::decodeThread(bool& exceptionThrown) const noexcept {
#ifdef HAVE_OPENMP
#pragma omp taskgroup
#pragma omp single
#endif
  createWaveletBandDecodingTasks(exceptionThrown);

  // Proceed only if decoding did not fail.
  if (!readValue(exceptionThrown)) {
    // And finally!
    combineFinalLowpassBands();
  }
}

void VC5Decompressor::decode(unsigned int offsetX, unsigned int offsetY,
                             unsigned int width, unsigned int height) {
  if (offsetX || offsetY || mRaw->dim != iPoint2D(width, height))
    ThrowRDE("VC5Decompressor expects to fill the whole image, not some tile.");

  initPrefixCodeDecoder();
  initVC5LogTable();

  alignas(RAWSPEED_CACHELINESIZE) bool exceptionThrown = false;

#ifdef HAVE_OPENMP
#pragma omp parallel default(none) shared(exceptionThrown)                     \
    num_threads(rawspeed_get_number_of_processor_cores())
#endif
  decodeThread(exceptionThrown);

  std::string firstErr;
  if (mRaw->isTooManyErrors(1, &firstErr)) {
    assert(exceptionThrown);
    ThrowRDE("Too many errors encountered. Giving up. First Error:\n%s",
             firstErr.c_str());
  } else {
    assert(!exceptionThrown);
  }
}

template <BayerPhase p>
void VC5Decompressor::combineFinalLowpassBandsImpl() const noexcept {
  const Array2DRef<uint16_t> out(mRaw->getU16DataAsUncroppedArray2DRef());

  const int width = out.width() / 2;
  const int height = out.height() / 2;

  assert(channels[0].wavelets[0].bands[0]->data.has_value() &&
         channels[1].wavelets[0].bands[0]->data.has_value() &&
         channels[2].wavelets[0].bands[0]->data.has_value() &&
         channels[3].wavelets[0].bands[0]->data.has_value() &&
         "Failed to reconstruct all final lowpass bands?");

  const Array2DRef<const int16_t> lowbands0 =
      channels[0].wavelets[0].bands[0]->data->description;
  const Array2DRef<const int16_t> lowbands1 =
      channels[1].wavelets[0].bands[0]->data->description;
  const Array2DRef<const int16_t> lowbands2 =
      channels[2].wavelets[0].bands[0]->data->description;
  const Array2DRef<const int16_t> lowbands3 =
      channels[3].wavelets[0].bands[0]->data->description;

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

      static constexpr BayerPhase basePhase = BayerPhase::RGGB;
      std::array<int, 4> patData = {r, g1, g2, b};

      for (int& patElt : patData)
        patElt = mVC5LogTable[patElt];

      patData = applyStablePhaseShift(patData, basePhase, p);

      const Array2DRef<const int> pat(patData.data(), 2, 2);
      for (int patRow = 0; patRow < pat.height(); ++patRow) {
        for (int patCol = 0; patCol < pat.width(); ++patCol) {
          out(2 * row + patRow, 2 * col + patCol) =
              static_cast<uint16_t>(pat(patRow, patCol));
        }
      }
    }
  }
}

void VC5Decompressor::combineFinalLowpassBands() const noexcept {
  switch (phase) {
    using enum BayerPhase;
  case RGGB:
    combineFinalLowpassBandsImpl<BayerPhase::RGGB>();
    return;
  case GBRG:
    combineFinalLowpassBandsImpl<BayerPhase::GBRG>();
    return;
  default:
    break;
  }
  __builtin_unreachable();
}

inline std::pair<int16_t /*value*/, unsigned int /*count*/>
VC5Decompressor::getRLV(const PrefixCodeDecoder& decoder,
                        BitStreamerMSB& bits) {
  unsigned bitfield = decoder.decodeCodeValue(bits);

  unsigned int count = bitfield & ((1U << RLVRunLengthBitWidth) - 1U);
  auto value = implicit_cast<int16_t>(bitfield >> RLVRunLengthBitWidth);

  if (value != 0 && bits.getBitsNoFill(1))
    value = -value;

  return {value, count};
}

void VC5Decompressor::Wavelet::AbstractBand::anchor() const {
  // Empty out-of-line definition for the purpose of anchoring
  // the class's vtable to this Translational Unit.
}

} // namespace rawspeed
