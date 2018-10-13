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
  pitch = waveletWidth * sizeof(int16_t);
  mDecodedBandMask = 0;

  data_storage.resize(numBands * waveletWidth * waveletHeight);
  for (int iBand = 0; iBand < numBands; ++iBand)
    data[iBand] = &data_storage[iBand * waveletWidth * waveletHeight];

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
VC5Decompressor::Wavelet::bandAsArray2DRef(const unsigned int iBand) const {
  return {data[iBand], width, height};
}

void VC5Decompressor::Wavelet::clear() {
  data_storage.clear();
  data_storage.shrink_to_fit();
  mInitialized = false;
  data.fill(nullptr);
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

void VC5Decompressor::Wavelet::reconstructLowband(
    Array2DRef<int16_t> dest, const int16_t prescale,
    const bool clampUint /* = false */) {
  unsigned int x, y;
  int16_t descaleShift = (prescale == 2 ? 2 : 0);
  // Assert valid quantization values
  if (quant[0] == 0)
    quant[0] = 1;
  for (int i = 0; i < numBands; ++i) {
    if (quant[i] == 0)
      ThrowRDE("Quant value of band %i must not be zero", i);
  }

  std::vector<int16_t> lowhigh_storage =
      Array2DRef<int16_t>::create(width, height);
  std::vector<int16_t> highlow_storage =
      Array2DRef<int16_t>::create(width, height);
  std::vector<int16_t> highhigh_storage =
      Array2DRef<int16_t>::create(width, height);

  std::vector<int16_t> lowpass_storage =
      Array2DRef<int16_t>::create(width, 2 * height);
  std::vector<int16_t> highpass_storage =
      Array2DRef<int16_t>::create(width, 2 * height);

  Array2DRef<int16_t> lowlow(data[0], width, height);
  Array2DRef<int16_t> lowhigh(lowhigh_storage.data(), width, height);
  Array2DRef<int16_t> highlow(highlow_storage.data(), width, height);
  Array2DRef<int16_t> highhigh(highhigh_storage.data(), width, height);

  Array2DRef<int16_t> lowpass(lowpass_storage.data(), width, 2 * height);
  Array2DRef<int16_t> highpass(highpass_storage.data(), width, 2 * height);

  dequantize(lowhigh, Array2DRef<int16_t>(data[1], width, height), quant[1]);
  dequantize(highlow, Array2DRef<int16_t>(data[2], width, height), quant[2]);
  dequantize(highhigh, Array2DRef<int16_t>(data[3], width, height), quant[3]);

  auto convolution = [&x, &y](std::array<int, 4> muls, Array2DRef<int16_t> high,
                              auto low, int DescaleShift = 0) {
    auto highCombined = muls[0] * high(x, y);
    auto lowsCombined = [muls, low]() {
      int lows = 0;
      for (int i = 0; i < 3; i++)
        lows += muls[1 + i] * low(i);
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

  // Vertical reconstruction
  // 1st row
  y = 0;
  for (x = 0; x < width; ++x) {
    static constexpr std::array<int, 4> even_muls = {+1, +11, -4, +1};
    int even = convolution(even_muls, highlow, [&x, &y, lowlow](int delta) {
      return lowlow(x, y + delta);
    });
    static constexpr std::array<int, 4> odd_muls = {-1, +5, +4, -1};
    int odd = convolution(odd_muls, highlow, [&x, &y, lowlow](int delta) {
      return lowlow(x, y + delta);
    });

    lowpass(x, 2 * y) = static_cast<int16_t>(even);
    lowpass(x, 2 * y + 1) = static_cast<int16_t>(odd);

    even = convolution(even_muls, highhigh, [&x, &y, lowhigh](int delta) {
      return lowhigh(x, y + delta);
    });
    odd = convolution(odd_muls, highhigh, [&x, &y, lowhigh](int delta) {
      return lowhigh(x, y + delta);
    });

    highpass(x, 2 * y) = static_cast<int16_t>(even);
    highpass(x, 2 * y + 1) = static_cast<int16_t>(odd);
  }
  // middle rows
  for (y = 1; y + 1 < height; ++y) {
    for (x = 0; x < width; ++x) {
      static constexpr std::array<int, 4> even_muls = {+1, +1, +8, -1};
      int even = convolution(even_muls, highlow, [&x, &y, lowlow](int delta) {
        return lowlow(x, y - 1 + delta);
      });
      static constexpr std::array<int, 4> odd_muls = {-1, -1, +8, +1};
      int odd = convolution(odd_muls, highlow, [&x, &y, lowlow](int delta) {
        return lowlow(x, y - 1 + delta);
      });

      lowpass(x, 2 * y) = static_cast<int16_t>(even);
      lowpass(x, 2 * y + 1) = static_cast<int16_t>(odd);

      even = convolution(even_muls, highhigh, [&x, &y, lowhigh](int delta) {
        return lowhigh(x, y - 1 + delta);
      });
      odd = convolution(odd_muls, highhigh, [&x, &y, lowhigh](int delta) {
        return lowhigh(x, y - 1 + delta);
      });

      highpass(x, 2 * y) = static_cast<int16_t>(even);
      highpass(x, 2 * y + 1) = static_cast<int16_t>(odd);
    }
  }
  // last row
  for (x = 0; x < width; ++x) {
    static constexpr std::array<int, 4> even_muls = {+1, +5, +4, -1};
    int even = convolution(even_muls, highlow, [&x, &y, lowlow](int delta) {
      return lowlow(x, y - delta);
    });
    static constexpr std::array<int, 4> odd_muls = {-1, +11, -4, +1};
    int odd = convolution(odd_muls, highlow, [&x, &y, lowlow](int delta) {
      return lowlow(x, y - delta);
    });

    lowpass(x, 2 * y) = static_cast<int16_t>(even);
    lowpass(x, 2 * y + 1) = static_cast<int16_t>(odd);

    even = convolution(even_muls, highhigh, [&x, &y, lowhigh](int delta) {
      return lowhigh(x, y - delta);
    });
    odd = convolution(odd_muls, highhigh, [&x, &y, lowhigh](int delta) {
      return lowhigh(x, y - delta);
    });

    highpass(x, 2 * y) = static_cast<int16_t>(even);
    highpass(x, 2 * y + 1) = static_cast<int16_t>(odd);
  }

  // Horizontal reconstruction
  for (y = 0; y < dest.height; ++y) {
    x = 0;

    // First col

    static constexpr std::array<int, 4> even_muls = {+1, +11, -4, +1};
    int even = convolution(
        even_muls, highpass,
        [&x, &y, lowpass](int delta) { return lowpass(x + delta, y); },
        descaleShift);
    static constexpr std::array<int, 4> odd_muls = {-1, +5, +4, -1};
    int odd = convolution(
        odd_muls, highpass,
        [&x, &y, lowpass](int delta) { return lowpass(x + delta, y); },
        descaleShift);

    if (clampUint) {
      even = clampBits(even, 14);
      odd = clampBits(odd, 14);
    }
    dest(2 * x, y) = static_cast<int16_t>(even);
    dest(2 * x + 1, y) = static_cast<int16_t>(odd);

    // middle cols
    for (x = 1; x + 1 < width; ++x) {
      static constexpr std::array<int, 4> middle_even_muls = {+1, +1, +8, -1};
      even = convolution(
          middle_even_muls, highpass,
          [&x, &y, lowpass](int delta) { return lowpass(x - 1 + delta, y); },
          descaleShift);
      static constexpr std::array<int, 4> middle_odd_muls = {-1, -1, +8, +1};
      odd = convolution(
          middle_odd_muls, highpass,
          [&x, &y, lowpass](int delta) { return lowpass(x - 1 + delta, y); },
          descaleShift);

      if (clampUint) {
        even = clampBits(even, 14);
        odd = clampBits(odd, 14);
      }
      dest(2 * x, y) = static_cast<int16_t>(even);
      dest(2 * x + 1, y) = static_cast<int16_t>(odd);
    }

    // last col
    static constexpr std::array<int, 4> last_even_muls = {+1, +5, +4, -1};
    even = convolution(
        last_even_muls, highpass,
        [&x, &y, lowpass](int delta) { return lowpass(x - delta, y); },
        descaleShift);

    if (clampUint)
      even = clampBits(even, 14);
    dest(2 * x, y) = static_cast<int16_t>(even);
    if (2 * x + 1 < dest.width) {
      static constexpr std::array<int, 4> last_odd_muls = {-1, +11, -4, +1};
      odd = convolution(
          last_odd_muls, highpass,
          [&x, &y, lowpass](int delta) { return lowpass(x - delta, y); },
          descaleShift);
      if (clampUint)
        odd = clampBits(odd, 14);
      dest(2 * x + 1, y) = static_cast<int16_t>(odd);
    }
  }
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
      for (int iWavelet = 0; iWavelet < Transform::numWavelets; ++iWavelet)
        mTransforms[mVC5.iChannel].prescale[iWavelet] =
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
      Wavelet& wavelet = mTransforms[iChannel].wavelet[0];
      if (!wavelet.isInitialized())
        done = false;
      if (!wavelet.allBandsValid())
        done = false;
    }
  }

  decodeFinalWavelet();
}

void VC5Decompressor::decodeLowPassBand(const ByteStream& bs,
                                        const Wavelet& wavelet) {
  BitPumpMSB bits(bs);
  auto wdata = wavelet.bandAsArray2DRef(0);
  for (int row = 0; row < wavelet.height; ++row) {
    for (int col = 0; col < wavelet.width; ++col) {
      wdata(col, row) =
          static_cast<int16_t>(bits.getBits(mVC5.lowpassPrecision));
    }
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
      wavelet->data[band][iPixel] = static_cast<int16_t>(pixelValue);
      ++iPixel;
    }
  }
  if (bits.getPosition() < bits.getSize()) {
    getRLV(&bits, &pixelValue, &count);
    if (pixelValue != MARKER_BAND_END || count != 0)
      ThrowRDE("EndOfBand marker not found");
  }
  wavelet->quant[band] = mVC5.quantization;
}

void VC5Decompressor::decodeLargeCodeblock(const ByteStream& bs) {
  Transform& transform = mTransforms[mVC5.iChannel];
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

  // Initialize wavelets
  uint16_t waveletWidth = roundUpDivision(channelWidth, 2);
  uint16_t waveletHeight = roundUpDivision(channelHeight, 2);
  for (Wavelet& wavelet : transform.wavelet) {
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

  Wavelet& wavelet = transform.wavelet[idx];
  if (mVC5.iSubband == 0) {
    assert(band == 0);
    decodeLowPassBand(bs, wavelet);
  } else {
    decodeHighPassBand(bs, band, &wavelet);
  }
  wavelet.setBandValid(band);

  // If this wavelet is fully decoded, reconstruct the low-pass band of
  // the next lower wavelet
  if (idx > 0 && wavelet.allBandsValid() &&
      !transform.wavelet[idx - 1].isBandValid(0)) {
    wavelet.reconstructLowband(transform.wavelet[idx - 1].bandAsArray2DRef(0),
                               transform.prescale[idx]);
    transform.wavelet[idx - 1].setBandValid(0);
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

  unsigned int width = 2 * mTransforms[0].wavelet[0].width;
  unsigned int height = 2 * mTransforms[0].wavelet[0].height;

  std::array<std::vector<int16_t>, numChannels> channels_storage;
  std::array<Array2DRef<int16_t>, numChannels> channels;
  for (unsigned int iChannel = 0; iChannel < numChannels; ++iChannel) {
    assert(2 * mTransforms[iChannel].wavelet[0].width == width);
    assert(2 * mTransforms[iChannel].wavelet[0].height == height);
    channels_storage[iChannel] = Array2DRef<int16_t>::create(width, height);
    channels[iChannel] =
        Array2DRef<int16_t>(channels_storage[iChannel].data(), width, height);
    mTransforms[iChannel].wavelet[0].reconstructLowband(
        channels[iChannel], mTransforms[iChannel].prescale[0], true);
  }

  // Convert to RGGB output
  // FIXME: this *should* be threadedable nicely.
  for (unsigned int row = 0; row < height; ++row) {
    for (unsigned int col = 0; col < width; ++col) {
      const int mid = 2048;

      int gs = channels[0](col, row);
      int rg = channels[1](col, row) - mid;
      int bg = channels[2](col, row) - mid;
      int gd = channels[3](col, row) - mid;

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
