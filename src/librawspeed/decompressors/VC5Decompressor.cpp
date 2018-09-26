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

#include "decompressors/VC5Decompressor.h"
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "io/Endianness.h"
#include <cmath>


// Definitions needed by table17.inc
// Taken from https://github.com/gopro/gpr/blob/a513701afce7b03173213a2f67dfd9dd28fa1868/source/lib/vc5_decoder/vlc.h
typedef struct _rlv {
  uint_fast8_t size;		//!< Size of code word in bits
  uint32_t bits;			//!< Code word bits right justified
  uint32_t count;			//!< Run length
  int32_t value;			//!< Run value (unsigned)
} RLV;
#define RLVTABLE(n)			\
  static struct			\
  {						\
    uint32_t length;	\
    RLV entries[n];		\
  }
#include "common/table17.inc"



#define VC5_TAG_ChannelCount 0x000c
#define VC5_TAG_ImageWidth 0x0014
#define VC5_TAG_ImageHeight 0x0015
#define VC5_TAG_LowpassPrecision 0x0023	//!< Number of bits per lowpass coefficient
#define VC5_TAG_SubbandNumber 0x0030		//!< Subband number of this wavelet band
#define VC5_TAG_Quantization 0x0035		//!< Quantization applied to band
#define VC5_TAG_ChannelNumber 0x003e		//!< Channel number
#define VC5_TAG_ImageFormat 0x0054
#define VC5_TAG_MaxBitsPerComponent 0x0066		//!< Upper bound on the number of bits per component
#define VC5_TAG_PatternWidth 0x006a //!< Number of samples per row in each pattern element
#define VC5_TAG_PatternHeight 0x006b //!< Number of rows of samples in each pattern element
#define VC5_TAG_ComponentsPerSample 0x006c //!< Number of components in each sample in the pattern element
#define VC5_TAG_PrescaleShift 0x006d		//!< Packed prescale shift for each wavelet level

#define VC5_TAG_LARGE_CHUNK 0x2000		//!< Large chunk with a 24-bit payload size (in segments)
#define VC5_TAG_SMALL_CHUNK 0x4000		//!< Small chunk with a 16-bit payload size (in segments)
#define VC5_TAG_UniqueImageIdentifier 0x4004	//!< Small chunk containing the identifier and sequence number for the image
#define VC5_TAG_LargeCodeblock 0x6000	//!< Large chunk that contains a codeblock

#define PRECISION_MIN 8
#define PRECISION_MAX 32

#define MARKER_BAND_END 1

namespace rawspeed {

template<class T>
VC5Decompressor::Array2D<T>::Array2D()
  : _pitch(0)
  , _data(nullptr)
  , width(0)
  , height(0)
{
}

template<class T>
VC5Decompressor::Array2D<T>::Array2D(T * data, const unsigned int dataWidth, const unsigned int dataHeight, const unsigned int dataPitch /* = 0 */)
  : _data(data)
  , width(dataWidth)
  , height(dataHeight)
{
  _pitch = (dataPitch == 0 ? dataWidth : dataPitch);
}
/*
// virtual
template<class T>
VC5Decompressor::Array2D<T>::~Array2D()
{
}
*/
// static
template<class T>
VC5Decompressor::Array2D<T> VC5Decompressor::Array2D<T>::create(const unsigned int width, const unsigned int height)
{
  return Array2D<T>(new T[width * height], width, height);
}

template<class T>
void VC5Decompressor::Array2D<T>::destroy()
{
  if (_data) delete[] _data;
  clear();
}

template<class T>
void VC5Decompressor::Array2D<T>::clear()
{
  _data = nullptr;
  width = height = _pitch = 0;
}

template<class T>
T & VC5Decompressor::Array2D<T>::operator()(const unsigned int x, const unsigned int y)
{
  assert(_data);
  assert(x < width);
  assert(y < height);
  return _data[y * _pitch + x];
}

template<class T>
T VC5Decompressor::Array2D<T>::operator()(const unsigned int x, const unsigned int y) const
{
  assert(_data);
  assert(x < width);
  assert(y < height);
  return _data[y * _pitch + x];
}

VC5Decompressor::Wavelet::Wavelet()
  : numBands(MAX_NUM_BANDS)
  , mDecodedBandMask(0)
  , mInitialized(false)
{
  for (int i = 0; i < MAX_NUM_BANDS; ++i) {
    data[i] = nullptr;
    quant[i] = 0;
    scale[i] = 0;
  }
}

void VC5Decompressor::Wavelet::initialize(uint16_t waveletWidth, uint16_t waveletHeight)
{
  this->width = waveletWidth;
  this->height = waveletHeight;
  numBands = MAX_NUM_BANDS;
  pitch = waveletWidth * sizeof(int16_t);
  mDecodedBandMask = 0;

  data[0] = new int16_t[MAX_NUM_BANDS * waveletWidth * waveletHeight];
  for (int iBand = 1; iBand < MAX_NUM_BANDS; ++iBand)
    data[iBand] = data[0] + iBand * waveletWidth * waveletHeight;

  mInitialized = true;
}

void VC5Decompressor::Wavelet::setBandValid(const int band)
{
  mDecodedBandMask |= (1 << band);
}

bool VC5Decompressor::Wavelet::isBandValid(const int band) const
{
  return mDecodedBandMask & (1 << band);
}

bool VC5Decompressor::Wavelet::allBandsValid() const
{
  return mDecodedBandMask == static_cast<uint32>((1 << numBands) - 1);
}

VC5Decompressor::Array2D<int16_t> VC5Decompressor::Wavelet::bandAsArray2D(const unsigned int iBand)
{
  return VC5Decompressor::Array2D<int16_t>(data[iBand], width, height);
}


void VC5Decompressor::Wavelet::clear()
{
  mInitialized = false;
  if (data[0])
    delete[] data[0];
  for (int i = 0; i < MAX_NUM_BANDS; ++i) {
    data[i] = nullptr;
  }
}

// static
void VC5Decompressor::Wavelet::dequantize(Array2D<int16_t> out, Array2D<int16_t> in, int16_t quant)
{
  for (unsigned int y = 0; y < in.height; ++y) {
    for (unsigned int x = 0; x < in.width; ++x) {
      double c = in(x, y);

      // Invert companding curve
      c += (c * c * c * 768) / (255.*255.*255.);

      out(x, y) = static_cast<int16_t>(c) * quant;
    }
  }
}

void VC5Decompressor::Wavelet::reconstructLowband(Array2D<int16_t> dest, const int16_t prescale)
{
  unsigned int x, y;
  int16_t descaleShift = (prescale == 2 ? 2 : 0);
  // Assert valid quantization values
  if (quant[0] == 0) quant[0] = 1;
  for (int i = 0; i < numBands; ++i) {
    if (quant[i] == 0) ThrowRDE("Quant value of band %i must not be zero", i);
  }

  Array2D<int16_t> lowlow(data[0], width, height);
  Array2D<int16_t> lowhigh = Array2D<int16_t>::create(width, height);
  Array2D<int16_t> highlow = Array2D<int16_t>::create(width, height);
  Array2D<int16_t> highhigh = Array2D<int16_t>::create(width, height);

  Array2D<int16_t> lowpass = Array2D<int16_t>::create(width, 2 * height);
  Array2D<int16_t> highpass = Array2D<int16_t>::create(width, 2 * height);

  dequantize(lowhigh, Array2D<int16_t>(data[1], width, height), quant[1]);
  dequantize(highlow, Array2D<int16_t>(data[2], width, height), quant[2]);
  dequantize(highhigh, Array2D<int16_t>(data[3], width, height), quant[3]);

  // Vertical reconstruction
  // 1st row
  y = 0;
  for (x = 0; x < width; ++x) {
    int even = (highlow(x, y) + ((11 * lowlow(x, y) - 4 * lowlow(x, y + 1) + lowlow(x, y + 2) + 4) >> 3)) >> 1;
    int odd = (-highlow(x, y) + ((5 * lowlow(x, y) + 4 * lowlow(x, y + 1) - lowlow(x, y + 2) + 4) >> 3)) >> 1;
    lowpass(x, 2 * y) = static_cast<int16_t>(even);
    lowpass(x, 2 * y + 1) = static_cast<int16_t>(odd);

    even = (highhigh(x, y) + ((11 * lowhigh(x, y) - 4 * lowhigh(x, y + 1) + lowhigh(x, y + 2) + 4) >> 3)) >> 1;
    odd = (-highhigh(x, y) + ((5 * lowhigh(x, y) + 4 * lowhigh(x, y + 1) - lowhigh(x, y + 2) + 4) >> 3)) >> 1;
    highpass(x, 2 * y) = static_cast<int16_t>(even);
    highpass(x, 2 * y + 1) = static_cast<int16_t>(odd);
  }
  // middle rows
  for (y = 1; y + 1 < height; ++y) {
    for (x = 0; x < width; ++x) {
      int even = (lowlow(x, y) + highlow(x, y) + ((lowlow(x, y - 1) - lowlow(x, y + 1) + 4) >> 3)) >> 1;
      int odd = (lowlow(x, y) - highlow(x, y) + ((lowlow(x, y + 1) - lowlow(x, y - 1) + 4) >> 3)) >> 1;
      lowpass(x, 2 * y) = static_cast<int16_t>(even);
      lowpass(x, 2 * y + 1) = static_cast<int16_t>(odd);

      even = (lowhigh(x, y) + highhigh(x, y) + ((lowhigh(x, y - 1) - lowhigh(x, y + 1) + 4) >> 3)) >> 1;
      odd = (lowhigh(x, y) - highhigh(x, y) + ((lowhigh(x, y + 1) - lowhigh(x, y - 1) + 4) >> 3)) >> 1;
      highpass(x, 2 * y) = static_cast<int16_t>(even);
      highpass(x, 2 * y + 1) = static_cast<int16_t>(odd);
    }
  }
  // last row
  for (x = 0; x < width; ++x) {
    int even = (highlow(x, y) + ((5 * lowlow(x, y) + 4 * lowlow(x, y - 1) - lowlow(x, y - 2) + 4) >> 3)) >> 1;
    int odd = (-highlow(x, y) + ((11 * lowlow(x, y) - 4 * lowlow(x, y - 1) + lowlow(x, y - 2) + 4) >> 3)) >> 1;
    lowpass(x, 2 * y) = static_cast<int16_t>(even);
    lowpass(x, 2 * y + 1) = static_cast<int16_t>(odd);

    even = (highhigh(x, y) + ((5 * lowhigh(x, y) + 4 * lowhigh(x, y - 1) - lowhigh(x, y - 2) + 4) >> 3)) >> 1;
    odd = (-highhigh(x, y) + ((11 * lowhigh(x, y) - 4 * lowhigh(x, y - 1) + lowhigh(x, y - 2) + 4) >> 3)) >> 1;
    highpass(x, 2 * y) = static_cast<int16_t>(even);
    highpass(x, 2 * y + 1) = static_cast<int16_t>(odd);
  }

  // Horizontal reconstruction
  for (y = 0; y < dest.height; ++y) {
    x = 0;

    // First col
    int even = ((highpass(x, y) + ((11 * lowpass(x, y) - 4 * lowpass(x + 1, y) + lowpass(x + 2, y) + 4) >> 3)) << descaleShift) >> 1;
    int odd = ((-highpass(x, y) + ((5 * lowpass(x, y) + 4 * lowpass(x + 1, y) - lowpass(x + 2, y) + 4) >> 3)) << descaleShift) >> 1;
    dest(2 * x, y) = static_cast<int16_t>(even);
    dest(2 * x + 1, y) = static_cast<int16_t>(odd);

    // middle cols
    for (x = 1; x + 1 < width; ++x) {
      even = ((highpass(x, y) + lowpass(x, y) + ((lowpass(x - 1, y) - lowpass(x + 1, y) + 4) >> 3)) << descaleShift) >> 1;
      odd = ((-highpass(x, y) + lowpass(x, y) + ((lowpass(x + 1, y) - lowpass(x - 1, y) + 4) >> 3)) << descaleShift) >> 1;
      dest(2 * x, y) = static_cast<int16_t>(even);
      dest(2 * x + 1, y) = static_cast<int16_t>(odd);
    }

    // last col
    even = ((highpass(x, y) + ((5 * lowpass(x, y) + 4 * lowpass(x - 1, y) - lowpass(x - 2, y) + 4) >> 3)) << descaleShift) >> 1;
    dest(2 * x, y) = static_cast<int16_t>(even);
    if (2 * x + 1 < dest.width) {
      odd = ((-highpass(x, y) + ((11 * lowpass(x, y) - 4 * lowpass(x - 1, y) + lowpass(x - 2, y) + 4) >> 3)) << descaleShift) >> 1;
      dest(2 * x + 1, y) = static_cast<int16_t>(even);
    }
  }

  lowhigh.destroy();
  highlow.destroy();
  highhigh.destroy();
  lowpass.destroy();
  highpass.destroy();
}

//inline
unsigned int VC5Decompressor::DecodeLog(const int val) const
{
  if (val < 0) return mVC5LogTable[0];
  if (val >= VC5_LOG_TABLE_SIZE) return mVC5LogTable[VC5_LOG_TABLE_SIZE - 1];
  return mVC5LogTable[val];
}

VC5Decompressor::VC5Decompressor(ByteStream bs, const RawImage& img)
 : AbstractDecompressor()
 , mImg(img)
 , mBs(bs)
{
  mVC5.numChannels = 0;
  mVC5.numSubbands = 10;
  mVC5.numWavelets = 3;
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
  mVC5.image_sequence_number = 0;
  mVC5.quantization = 0;

  mVC5LogTable = new unsigned int[VC5_LOG_TABLE_SIZE];
  for (int i = 0; i < VC5_LOG_TABLE_SIZE; ++i)
    mVC5LogTable[i] = static_cast<unsigned int>(img->whitePoint * (std::pow(113.0, i / (VC5_LOG_TABLE_SIZE - 1.)) - 1) / 112.);
}

//virtual
VC5Decompressor::~VC5Decompressor()
{
  if (mVC5LogTable)
    delete[] mVC5LogTable;
}


void VC5Decompressor::decode(const unsigned int offsetX, const unsigned int offsetY)
{
  unsigned int chunkSize = 0;
  mBs.setByteOrder(Endianness::big);

  assert(mImg->dim.x > 0);
  assert(mImg->dim.y > 0);

  // All VC-5 data must start with "VC-%" (0x56432d35)
  if (mBs.getU32() != 0x56432d35)
    ThrowRDE("not a valid VC-5 datablock");

  bool done = false;
  while (!done) {
    int16_t tag = static_cast<int16_t>(mBs.getU16());
    ushort16 val = mBs.getU16();

    bool optional;
    if (tag < 0) {
      tag = -tag;
      optional = true;
    }
    else
      optional = false;

    switch(tag) {
      case VC5_TAG_ChannelCount:
        mVC5.numChannels = val;
        break;
    case VC5_TAG_ImageWidth:
      mVC5.imgWidth = val;
      break;
    case VC5_TAG_ImageHeight:
      mVC5.imgHeight = val;
      break;
    case VC5_TAG_LowpassPrecision:
      if (val < PRECISION_MIN || val > PRECISION_MAX) ThrowRDE("Invalid precision %i", val);
      mVC5.lowpassPrecision = val;
      break;
    case VC5_TAG_ChannelNumber:
      mVC5.iChannel = val;
      break;
     case VC5_TAG_ImageFormat:
      if (val != 4)
        ThrowRDE("Image format %i is not 4(RAW)", val);
      mVC5.imgFormat = val; // 4=RAW
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
      mVC5.iSubband = val;
      break;
    case VC5_TAG_Quantization:
      mVC5.quantization = static_cast<short16>(val);
      break;
    case VC5_TAG_ComponentsPerSample:
     mVC5.cps = val;
     break;
    case VC5_TAG_PrescaleShift:
      for (int iWavelet = 0; iWavelet < MAX_NUM_WAVELETS; ++iWavelet)
        mTransforms[mVC5.iChannel].prescale[iWavelet] = (val >> (14 - 2 * iWavelet)) & 0x03;
      break;
    default:
        if (tag & VC5_TAG_LARGE_CHUNK)
          chunkSize = static_cast<unsigned int>(((tag & 0xff) << 16) | (val & 0xffff));
        else if (tag & VC5_TAG_SMALL_CHUNK)
          chunkSize = (val & 0xffff);

        if ((tag & VC5_TAG_LargeCodeblock) == VC5_TAG_LargeCodeblock) {
          Transform & transform = mTransforms[mVC5.iChannel];
          static int subband_wavelet_index[] = {2, 2, 2, 2, 1, 1, 1, 0, 0, 0};
          static int subband_band_index[] = {0, 1, 2, 3, 1, 2, 3, 1, 2, 3};
          const int idx = subband_wavelet_index[mVC5.iSubband];
          const int band = subband_band_index[mVC5.iSubband];
          uint16_t channelWidth = mVC5.imgWidth / mVC5.patternWidth;
          uint16_t channelHeight = mVC5.imgHeight / mVC5.patternHeight;

          if (mVC5.patternWidth != 2 || mVC5.patternHeight != 2)
            ThrowRDE("Invalid RAW file, pattern size != 2x2");

          // Initialize wavelets
          uint16_t waveletWidth = ((channelWidth % 2) == 0 ? channelWidth : channelWidth + 1) / 2;
          uint16_t waveletHeight = ((channelHeight % 2) == 0 ? channelHeight : channelHeight + 1) / 2;
          for (int iWavelet = 0; iWavelet < mVC5.numWavelets; ++iWavelet) {
            Wavelet & wavelet = transform.wavelet[iWavelet];
            if (wavelet.isInitialized()) {
              if (wavelet.width != waveletWidth || wavelet.height != waveletHeight)
                wavelet.clear();
            }
            if (!wavelet.isInitialized())
              wavelet.initialize(waveletWidth, waveletHeight);

            // Pad dimensions as necessary and divide them by two for the next
            // wavelet
            if ((waveletWidth % 2) != 0) ++waveletWidth;
            if ((waveletHeight % 2) != 0) ++waveletHeight;
            waveletWidth /= 2;
            waveletHeight /= 2;
          }

          BitPumpMSB bits(mBs);
          // Even for BitPump's, getPosition() returns full byte positions
          // (rounded up)
          BitPumpMSB::size_type startPos = bits.getPosition();
          Wavelet & wavelet = transform.wavelet[idx];
          if (mVC5.iSubband == 0) {
            // decode lowpass band
            assert(band == 0);
            for (int row = 0; row < wavelet.height; ++row) {
              for (int col = 0; col < wavelet.width; ++col)
                wavelet.data[0][row * wavelet.width + col] = static_cast<int16_t>(bits.getBits(mVC5.lowpassPrecision));
            }
            wavelet.setBandValid(0);
          }
          else {
            // decode highpass band
            int pixelValue = 0;
            unsigned int count = 0;
            int nPixels = wavelet.width * wavelet.height;
            for (int iPixel = 0; iPixel < nPixels; ) {
              getRLV(bits, pixelValue, count);
              for (; count > 0; --count) {
                if (iPixel > nPixels) ThrowRDE("Buffer overflow");
                wavelet.data[band][iPixel] = static_cast<int16_t>(pixelValue);
                ++iPixel;
              }
            }
            if (bits.getPosition() < bits.getSize()) {
              getRLV(bits, pixelValue, count);
              if (pixelValue != MARKER_BAND_END || count != 0) ThrowRDE("EndOfBand marker not found");
            }
            wavelet.setBandValid(band);
            wavelet.quant[band] = mVC5.quantization;
          }
          mBs.skipBytes(bits.getPosition() - startPos);

          // If this wavelet is fully decoded, reconstruct the low-pass band of
          // the next lower wavelet
          if (idx > 0 && wavelet.allBandsValid() && !transform.wavelet[idx - 1].isBandValid(0)) {
            wavelet.reconstructLowband(transform.wavelet[idx - 1].bandAsArray2D(0), transform.prescale[idx]);
            transform.wavelet[idx - 1].setBandValid(0);
          }

          mVC5.iSubband++;
          if (mVC5.iSubband == mVC5.numSubbands) {
            mVC5.iChannel++;
            mVC5.iSubband = 0;
          }
        }
        else if (tag == VC5_TAG_UniqueImageIdentifier) {
          if (!optional) ThrowRDE("UniqueImageIdentifier tag should be optional");
          if (val != 9) ThrowRDE("UniqueImageIdentifier must have a payload of 9 segments/36 bytes (%i segments encountered)", val);
          if (memcmp(mBs.getData(12), "\x06\x0a\x2b\x34\x01\x01\x01\x05\x01\x01\x01\x20", 12) != 0)
             ThrowRDE("UniqueImageIdentifier should start with a UMID label");
          if (mBs.getByte() != 0x13) ThrowRDE("UMID length ist not 0x13");
          if (memcmp(mBs.getData(3), "\x00\x00\x00", 3) != 0) ThrowRDE("UMID instance number is not 0");

          memcpy(mVC5.image_sequence_identifier, mBs.getData(sizeof(mVC5.image_sequence_identifier)), sizeof(mVC5.image_sequence_identifier));
          mVC5.image_sequence_number = mBs.getU32();
        }
/*
        else if (tag == CODEC_TAG_InverseTransform)
          else if (tag == CODEC_TAG_InversePermutation)
          else if (tag == CODEC_TAG_InverseTransform16)
          else if (IsPartEnabled(enabled_parts, VC5_PART_SECTIONS) && decoder->section_flag && IsSectionHeader(tag))
*/

        else {
//          printf("Encountered unknown tag 0x%04x @ offset 0x%x\n", tag, mBs.getPosition());
          if (tag & VC5_TAG_LARGE_CHUNK) {
            optional = true;
            chunkSize = 0;
          }
          if (!optional) ThrowRDE("Tag 0x%04x should be optional", tag);
          else if (chunkSize > 0)
            mBs.skipBytes(4 * chunkSize); // chunkSize is in units of uint32
        }
    }

    done = true;
    for (int iChannel = 0; iChannel < mVC5.numChannels && done; ++iChannel) {
      Wavelet & wavelet = mTransforms[iChannel].wavelet[0];
      if (!wavelet.isInitialized()) done = false;
      if (!wavelet.allBandsValid()) done = false;
    }
  }

  // Decode final wavelet into image
  Array2D<uint16_t> out(reinterpret_cast<uint16_t*>(mImg->getData()), static_cast<unsigned int>(mImg->dim.x), static_cast<unsigned int>(mImg->dim.y), mImg->pitch / sizeof(uint16_t));

  unsigned int width = 2 * mTransforms[0].wavelet[0].width;
  unsigned int height = 2 * mTransforms[0].wavelet[0].height;

  Array2D<int16_t> channels[4];
  for (unsigned int iChannel = 0; iChannel < MAX_NUM_CHANNELS; ++iChannel) {
    assert(2 * mTransforms[iChannel].wavelet[0].width == width);
    assert(2 * mTransforms[iChannel].wavelet[0].height == height);
    channels[iChannel] = Array2D<int16_t>::create(width, height);
    mTransforms[iChannel].wavelet[0].reconstructLowband(channels[iChannel], mTransforms[iChannel].prescale[0]);
  }

  // Convert to RGGB output
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

      out(2 * col + 0, 2 * row + 0) = static_cast<uint16_t>(DecodeLog(r));
      out(2 * col + 1, 2 * row + 0) = static_cast<uint16_t>(DecodeLog(g1));
      out(2 * col + 0, 2 * row + 1) = static_cast<uint16_t>(DecodeLog(g2));
      out(2 * col + 1, 2 * row + 1) = static_cast<uint16_t>(DecodeLog(b));
    }
  }
}

//static
void VC5Decompressor::getRLV(BitPumpMSB & bits, int & value, unsigned int & count)
{
  unsigned int iTab;

  // Ensure the maximum number of bits are cached to make peekBits() as fast as
  // possible.
  bits.fill(table17.entries[table17.length - 1].size);
  for (iTab = 0; iTab < table17.length; ++iTab) {
    if (table17.entries[iTab].bits == bits.peekBits(table17.entries[iTab].size))
      break;
  }
  if (iTab >= table17.length) ThrowRDE("Code not found in codebook");

  bits.skipBits(table17.entries[iTab].size);
  value = table17.entries[iTab].value;
  count = table17.entries[iTab].count;
  if (value != 0) {
    if (bits.getBits(1)) value = -value;
  }
}

} // namespace rawspeed
