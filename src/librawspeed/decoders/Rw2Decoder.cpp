/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2014 Pedro Côrte-Real

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

#include "decoders/Rw2Decoder.h"
#include "adt/Array1DRef.h"
#include "adt/Array1DRefExtras.h"
#include "adt/Array2DRef.h"
#include "adt/Point.h"
#include "bitstreams/BitStreams.h"
#include "codes/AbstractPrefixCode.h"
#include "codes/PrefixCode.h"
#include "common/BayerPhase.h"
#include "common/Common.h"
#include "common/RawImage.h"
#include "decoders/RawDecoderException.h"
#include "decompressors/PanasonicV4Decompressor.h"
#include "decompressors/PanasonicV5Decompressor.h"
#include "decompressors/PanasonicV6Decompressor.h"
#include "decompressors/PanasonicV7Decompressor.h"
#include "decompressors/PanasonicV8Decompressor.h"
#include "decompressors/UncompressedDecompressor.h"
#include "io/Buffer.h"
#include "io/ByteStream.h"
#include "io/Endianness.h"
#include "metadata/Camera.h"
#include "metadata/ColorFilterArray.h"
#include "tiff/TiffEntry.h"
#include "tiff/TiffIFD.h"
#include "tiff/TiffTag.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using std::fabs;

namespace rawspeed {

class CameraMetaData;

bool Rw2Decoder::isAppropriateDecoder(const TiffRootIFD* rootIFD,
                                      [[maybe_unused]] Buffer file) {
  const auto id = rootIFD->getID();
  const std::string& make = id.make;

  // FIXME: magic

  return make == "Panasonic" || make == "LEICA" || make == "LEICA CAMERA AG";
}

namespace {

/// Retrieve list of values from Panasonic TiffTag
template <typename T>
void getPanasonicTiffVector(const TiffIFD& ifd, TiffTag tag,
                            std::vector<T>& output) {
  ByteStream bs = ifd.getEntry(tag)->getData();
  output.resize(bs.getU16());

  // Note: Relying on ByteStream and its parent classes to prevent out-of-bounds
  // reading.
  for (T& v : output)
    v = bs.get<T>();
}

/// Decompressor parameters populated from tags. They remain constant after
/// construction.
struct DecompressorV8Params {
  std::vector<uint32_t> stripByteOffsets;
  std::vector<uint32_t> stripLineOffsets;
  std::vector<uint32_t> stripBitLengths;
  std::vector<uint16_t> stripWidths;
  std::vector<uint16_t> stripHeights;
  uint16_t horizontalStripCount;
  uint16_t verticalStripCount;

  PanasonicV8Decompressor::Bayer2x2 initialPrediction;

  /// Huffman decoding shift down value. Appears to be unused.
  std::vector<uint16_t> huffShiftDown;

  uint16_t gammaClipVal;

  void validate() const;

  DecompressorV8Params() = delete;

  explicit DecompressorV8Params(const TiffIFD& ifd);
};

void DecompressorV8Params::validate() const {
  const unsigned totalStrips = horizontalStripCount * verticalStripCount;

  // Check that we won't be going OOB on any of these strip lists
  if (totalStrips > stripByteOffsets.size())
    ThrowRDE("Strip byte offset list does not have enough entries for the "
             "number of strips!");
  if (totalStrips > stripWidths.size())
    ThrowRDE("Strip widths list does not have enough entries for the number of "
             "strips!");
  if (totalStrips > stripHeights.size())
    ThrowRDE("Strip heights list does not have enough entries for the number "
             "of strips!");
  if (totalStrips > stripLineOffsets.size())
    ThrowRDE("Strip line offset list does not have enough entries for the "
             "number of strips!");
  if (totalStrips > stripBitLengths.size())
    ThrowRDE("Strip bit length list does not have enough entries for the "
             "number of strips!");

  if (std::any_of(huffShiftDown.begin(), huffShiftDown.end(),
                  [](uint16_t x) { return x != 0; })) {
    ThrowRDE("Non-zero shift down value encountered! Shift down decoding has "
             "never been tested!");
  }

  if (gammaClipVal != std::numeric_limits<uint16_t>::max()) {
    ThrowRDE("Got non-no-op gammaClipVal (%u). Not known to happen "
             "in-the-wild. Please file a bug!",
             gammaClipVal);
  }
}

DecompressorV8Params::DecompressorV8Params(const TiffIFD& ifd) {
  // NOLINTBEGIN(cppcoreguidelines-prefer-member-initializer)
  horizontalStripCount =
      ifd.getEntry(TiffTag::PANASONIC_V8_NUMBER_OF_STRIPS_H)->getU16();
  verticalStripCount =
      ifd.getEntry(TiffTag::PANASONIC_V8_NUMBER_OF_STRIPS_V)->getU16();

  getPanasonicTiffVector(ifd, TiffTag::PANASONIC_V8_STRIP_BYTE_OFFSETS,
                         stripByteOffsets);
  getPanasonicTiffVector(ifd, TiffTag::PANASONIC_V8_STRIP_LINE_OFFSETS,
                         stripLineOffsets);
  getPanasonicTiffVector(ifd, TiffTag::PANASONIC_V8_STRIP_DATA_SIZE,
                         stripBitLengths);
  getPanasonicTiffVector(ifd, TiffTag::PANASONIC_V8_STRIP_WIDTHS, stripWidths);
  getPanasonicTiffVector(ifd, TiffTag::PANASONIC_V8_STRIP_HEIGHTS,
                         stripHeights);

  // Get decoder's initial prediction value:
  // Note, the positions of the green samples are swapped. This is intentional,
  // the original implementation did this each swap redundantly during decoding
  // of each tile.
  initialPrediction[0] =
      ifd.getEntry(TiffTag::PANASONIC_V8_INIT_PRED_RED)->getU16();
  initialPrediction[2] =
      ifd.getEntry(TiffTag::PANASONIC_V8_INIT_PRED_GREEN1)->getU16();
  initialPrediction[1] =
      ifd.getEntry(TiffTag::PANASONIC_V8_INIT_PRED_GREEN2)->getU16();
  initialPrediction[3] =
      ifd.getEntry(TiffTag::PANASONIC_V8_INIT_PRED_BLUE)->getU16();

  getPanasonicTiffVector(ifd, TiffTag::PANASONIC_V8_HUF_SHIFT_DOWN,
                         huffShiftDown);

  gammaClipVal = ifd.getEntry(TiffTag::PANASONIC_V8_CLIP_VAL)->getU16();
  // NOLINTEND(cppcoreguidelines-prefer-member-initializer)

  validate();
}

PanasonicV8Decompressor::PrefixCodeDecoder
populatePrefixCodeDecoder(const TiffIFD& ifd) {
  using Tag = PanasonicV8Decompressor::PrefixCodeDecoder::Tag;

  using CodeSymbol = AbstractPrefixCode<Tag>::CodeSymbol;
  using CodeValueTy = CodeTraits<Tag>::CodeValueTy;

  ByteStream stream = ifd.getEntry(TiffTag::PANASONIC_V8_HUF_TABLE)->getData();
  const int numCodeSymbols = stream.getU16();

  std::vector<CodeSymbol> symbols;
  std::vector<CodeValueTy> codeValues;

  symbols.reserve(numCodeSymbols);
  codeValues.reserve(numCodeSymbols);
  for (int i = 0; i != numCodeSymbols; ++i) {
    const auto len = stream.getU16();
    const auto bits = stream.getU16();
    symbols.emplace_back(bits, len);
    codeValues.emplace_back(i);
  }

  PrefixCode<Tag> code(std::move(symbols), std::move(codeValues));
  PanasonicV8Decompressor::PrefixCodeDecoder codeDecoder(std::move(code));
  codeDecoder.setup(/*fullDecode_=*/true, /*fixDNGBug16_=*/false);
  return codeDecoder;
}

/// Maybe the most complicated part of the entire file format, and seemingly,
/// completely unused.
void populateGammaLUT(const DecompressorV8Params& mParams, const TiffIFD& ifd) {
  std::vector<uint16_t> mGammaLUT;

  // Retrieve encoded gamma curve from tags.
  std::vector<uint32_t> encodedGammaPoints;
  std::vector<uint32_t> encodedGammaSlopes;
  getPanasonicTiffVector(ifd, TiffTag::PANASONIC_V8_GAMMA_POINTS,
                         encodedGammaPoints);
  getPanasonicTiffVector(ifd, TiffTag::PANASONIC_V8_GAMMA_SLOPES,
                         encodedGammaSlopes);

  // Determine if the points and slopes are all set to zero and 65536
  // respectively. If so, no gamma function needs to be applied. This is
  // currently true of all tested RW2 files.
  const bool gamamPointsAreIdentity =
      std::all_of(encodedGammaPoints.cbegin(), encodedGammaPoints.cend(),
                  [](const uint32_t p) { return p == 0U; });
  const bool gammaSlopesAreIdentity =
      std::all_of(encodedGammaSlopes.cbegin(), encodedGammaSlopes.cend(),
                  [](const uint32_t s) { return s == 65536U; });

  if (!gamamPointsAreIdentity || !gammaSlopesAreIdentity) {
    // Generate gamma LUT based on retrieved curve.
    ThrowRDE("Non-identity gamma curve encountered. Never encountered in any "
             "testing samples!");

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunreachable-code"
    if (encodedGammaPoints.size() != 6 || encodedGammaSlopes.size() != 6) {
      ThrowRDE("Gamma curve point and/or slope list is not the expected length "
               "of 6");
    }
#pragma GCC diagnostic pop
  }
}

std::vector<Array1DRef<const uint8_t>>
getInputStrips(const DecompressorV8Params& mParams, Buffer mInputFile) {
  std::vector<Array1DRef<const uint8_t>> mStrips;

  const int totalStrips =
      mParams.horizontalStripCount * mParams.verticalStripCount;

  for (int stripIdx = 0; stripIdx < totalStrips; ++stripIdx) {
    const uint32_t stripSize = (mParams.stripBitLengths[stripIdx] + 7) / 8;
    const uint32_t stripOffset = mParams.stripByteOffsets[stripIdx];

    // Note: Relying on Buffer to catch OOB access attempts
    DataBuffer stripBuffer(mInputFile.getSubView(stripOffset, stripSize),
                           Endianness::big);
    mStrips.emplace_back(stripBuffer.getAsArray1DRef());
  }

  return mStrips;
}

} // namespace

RawImage Rw2Decoder::decodeRawV8(const TiffIFD& raw) const {
  parseCFA();
  if (getAsBayerPhase(mRaw->cfa) != BayerPhase::RGGB)
    ThrowRDE("Unexpected CFA, only RGGB is supported");

  const DecompressorV8Params mParams(raw);
  const auto mPrefixCodeDecoder = populatePrefixCodeDecoder(raw);
  populateGammaLUT(mParams, raw);
  const std::vector<Array1DRef<const uint8_t>> mStrips =
      getInputStrips(mParams, mFile);

  const auto imgDim = iRectangle2D({0, 0}, mRaw->dim);

  PanasonicV8Decompressor::DecompressorParamsBuilder b(
      imgDim, mParams.initialPrediction, getAsArray1DRef(mStrips),
      getAsArray1DRef(mParams.stripLineOffsets),
      getAsArray1DRef(mParams.stripWidths),
      getAsArray1DRef(mParams.stripHeights));

  PanasonicV8Decompressor v8(mRaw, b.getDecompressorParams(),
                             mPrefixCodeDecoder);
  mRaw->createData();
  v8.decompress();
  return mRaw;
}

RawImage Rw2Decoder::decodeRawInternal() {

  const TiffIFD* raw = nullptr;
  bool isOldPanasonic =
      !mRootIFD->hasEntryRecursive(TiffTag::PANASONIC_STRIPOFFSET);

  if (!isOldPanasonic)
    raw = mRootIFD->getIFDWithTag(TiffTag::PANASONIC_STRIPOFFSET);
  else
    raw = mRootIFD->getIFDWithTag(TiffTag::STRIPOFFSETS);

  uint32_t height = raw->getEntry(static_cast<TiffTag>(3))->getU16();
  uint32_t width = raw->getEntry(static_cast<TiffTag>(2))->getU16();

  if (isOldPanasonic) {
    if (width == 0 || height == 0 || width > 4330 || height > 2751)
      ThrowRDE("Unexpected image dimensions found: (%u; %u)", width, height);

    const TiffEntry* offsets = raw->getEntry(TiffTag::STRIPOFFSETS);

    if (offsets->count != 1) {
      ThrowRDE("Multiple Strips found: %u", offsets->count);
    }
    uint32_t offset = offsets->getU32();
    if (!mFile.isValid(offset))
      ThrowRDE("Invalid image data offset, cannot decode.");

    mRaw->dim = iPoint2D(width, height);

    uint32_t size = mFile.getSize() - offset;

    if (size >= width * height * 2) {
      // It's completely unpacked little-endian
      UncompressedDecompressor u(
          ByteStream(DataBuffer(mFile.getSubView(offset), Endianness::little)),
          mRaw, iRectangle2D({0, 0}, iPoint2D(width, height)), 16 * width / 8,
          16, BitOrder::LSB);
      mRaw->createData();
      u.decode12BitRawUnpackedLeftAligned<Endianness::little>();
    } else if (size >= width * height * 3 / 2) {
      // It's a packed format
      UncompressedDecompressor u(
          ByteStream(DataBuffer(mFile.getSubView(offset), Endianness::little)),
          mRaw, iRectangle2D({0, 0}, iPoint2D(width, height)),
          (12 * width / 8) + ((width + 2) / 10), 12, BitOrder::LSB);
      mRaw->createData();
      u.decode12BitRawWithControl<Endianness::little>();
    } else {
      uint32_t section_split_offset = 0;
      PanasonicV4Decompressor p(
          mRaw,
          ByteStream(DataBuffer(mFile.getSubView(offset), Endianness::little)),
          hints.contains("zero_is_not_bad"), section_split_offset);
      mRaw->createData();
      p.decompress();
    }
  } else {
    mRaw->dim = iPoint2D(width, height);

    const TiffEntry* offsets = raw->getEntry(TiffTag::PANASONIC_STRIPOFFSET);

    if (offsets->count != 1) {
      ThrowRDE("Multiple Strips found: %u", offsets->count);
    }

    uint32_t offset = offsets->getU32();

    ByteStream bs(DataBuffer(mFile.getSubView(offset), Endianness::little));

    uint16_t bitsPerSample = 12;
    if (raw->hasEntry(TiffTag::PANASONIC_BITSPERSAMPLE))
      bitsPerSample = raw->getEntry(TiffTag::PANASONIC_BITSPERSAMPLE)->getU16();

    switch (uint16_t version =
                raw->getEntry(TiffTag::PANASONIC_RAWFORMAT)->getU16()) {
    case 4: {
      uint32_t section_split_offset = 0x1FF8;
      PanasonicV4Decompressor p(mRaw, bs, hints.contains("zero_is_not_bad"),
                                section_split_offset);
      mRaw->createData();
      p.decompress();
      return mRaw;
    }
    case 5: {
      PanasonicV5Decompressor v5(mRaw, bs, bitsPerSample);
      mRaw->createData();
      v5.decompress();
      return mRaw;
    }
    case 6: {
      if (bitsPerSample != 14 && bitsPerSample != 12)
        ThrowRDE("Version %i: unexpected bits per sample: %i", version,
                 bitsPerSample);

      PanasonicV6Decompressor v6(mRaw, bs, bitsPerSample);
      mRaw->createData();
      v6.decompress();
      return mRaw;
    }
    case 7: {
      if (bitsPerSample != 14)
        ThrowRDE("Version %i: unexpected bits per sample: %i", version,
                 bitsPerSample);
      PanasonicV7Decompressor v7(mRaw, bs);
      mRaw->createData();
      v7.decompress();
      return mRaw;
    }
    case 8: {
      // Known values are 12, 14, and 16. Other less than 16 should decompress
      // fine.
      if (bitsPerSample > 16)
        ThrowRDE("Version %i: unexpected bits per sample: %i", version,
                 bitsPerSample);
      return decodeRawV8(*raw);
    }
    default:
      ThrowRDE("Version %i is unsupported", version);
    }
  }

  return mRaw;
}

void Rw2Decoder::checkSupportInternal(const CameraMetaData* meta) {
  auto id = mRootIFD->getID();
  if (!checkCameraSupported(meta, id, guessMode()))
    checkCameraSupported(meta, id, "");
}

void Rw2Decoder::parseCFA() const {
  if (!mRootIFD->hasEntryRecursive(TiffTag::PANASONIC_CFAPATTERN))
    ThrowRDE("No PANASONIC_CFAPATTERN entry found!");

  const TiffEntry* CFA =
      mRootIFD->getEntryRecursive(TiffTag::PANASONIC_CFAPATTERN);
  if (CFA->type != TiffDataType::SHORT || CFA->count != 1) {
    ThrowRDE("Bad PANASONIC_CFAPATTERN entry (type %u, count %u).",
             static_cast<unsigned>(CFA->type), CFA->count);
  }

  switch (auto i = CFA->getU16()) {
    using enum CFAColor;
  case 1:
    mRaw->cfa.setCFA(iPoint2D(2, 2), RED, GREEN, GREEN, BLUE);
    break;
  case 2:
    mRaw->cfa.setCFA(iPoint2D(2, 2), GREEN, RED, BLUE, GREEN);
    break;
  case 3:
    mRaw->cfa.setCFA(iPoint2D(2, 2), GREEN, BLUE, RED, GREEN);
    break;
  case 4:
    mRaw->cfa.setCFA(iPoint2D(2, 2), BLUE, GREEN, GREEN, RED);
    break;
  default:
    ThrowRDE("Unexpected CFA pattern: %u", i);
  }
}

const TiffIFD* Rw2Decoder::getRaw() const {
  return mRootIFD->hasEntryRecursive(TiffTag::PANASONIC_STRIPOFFSET)
             ? mRootIFD->getIFDWithTag(TiffTag::PANASONIC_STRIPOFFSET)
             : mRootIFD->getIFDWithTag(TiffTag::STRIPOFFSETS);
}

void Rw2Decoder::decodeMetaDataInternal(const CameraMetaData* meta) {
  parseCFA();

  auto id = mRootIFD->getID();
  std::string mode = guessMode();
  int iso = 0;
  if (mRootIFD->hasEntryRecursive(TiffTag::PANASONIC_ISO_SPEED))
    iso = mRootIFD->getEntryRecursive(TiffTag::PANASONIC_ISO_SPEED)->getU32();

  if (this->checkCameraSupported(meta, id, mode)) {
    setMetaData(meta, id, mode, iso);
  } else {
    mRaw->metadata.mode = mode;
    writeLog(DEBUG_PRIO::EXTRA, "Mode not found in DB: %s", mode.c_str());
    setMetaData(meta, id, "", iso);
  }

  const TiffIFD* raw = getRaw();

  // Read blacklevels
  if (raw->hasEntry(static_cast<TiffTag>(0x1c)) &&
      raw->hasEntry(static_cast<TiffTag>(0x1d)) &&
      raw->hasEntry(static_cast<TiffTag>(0x1e))) {
    auto blackLevelsNeedOffsetting = [&]() {
      bool isOldPanasonic =
          !mRootIFD->hasEntryRecursive(TiffTag::PANASONIC_STRIPOFFSET);
      if (isOldPanasonic)
        return true;
      const uint16_t version =
          raw->getEntry(TiffTag::PANASONIC_RAWFORMAT)->getU16();
      // After version 4 the black levels appears to be correct.
      return version <= 4;
    };
    const auto getBlack = [&raw, blackLevelsNeedOffsetting](TiffTag t) {
      const int val = raw->getEntry(t)->getU16();
      if (!blackLevelsNeedOffsetting())
        return val;
      // Continue adding 15 for older raw versions.
      int out;
      if (__builtin_sadd_overflow(val, 15, &out))
        ThrowRDE("Integer overflow when calculating black level");
      return out;
    };

    const int blackRed = getBlack(static_cast<TiffTag>(0x1c));
    const int blackGreen = getBlack(static_cast<TiffTag>(0x1d));
    const int blackBlue = getBlack(static_cast<TiffTag>(0x1e));

    mRaw->blackLevelSeparate =
        Array2DRef(mRaw->blackLevelSeparateStorage.data(), 2, 2);
    auto blackLevelSeparate1D = *mRaw->blackLevelSeparate->getAsArray1DRef();
    for (int i = 0; i < 2; i++) {
      for (int j = 0; j < 2; j++) {
        const int k = i + (2 * j);
        const CFAColor c = mRaw->cfa.getColorAt(i, j);
        switch (c) {
        case CFAColor::RED:
          blackLevelSeparate1D(k) = blackRed;
          break;
        case CFAColor::GREEN:
          blackLevelSeparate1D(k) = blackGreen;
          break;
        case CFAColor::BLUE:
          blackLevelSeparate1D(k) = blackBlue;
          break;
        default:
          ThrowRDE("Unexpected CFA color %s.",
                   ColorFilterArray::colorToString(c).c_str());
        }
      }
    }
  }

  // Read WB levels
  if (raw->hasEntry(static_cast<TiffTag>(0x0024)) &&
      raw->hasEntry(static_cast<TiffTag>(0x0025)) &&
      raw->hasEntry(static_cast<TiffTag>(0x0026))) {
    std::array<float, 4> wbCoeffs = {};
    wbCoeffs[0] = static_cast<float>(
        raw->getEntry(static_cast<TiffTag>(0x0024))->getU16());
    wbCoeffs[1] = static_cast<float>(
        raw->getEntry(static_cast<TiffTag>(0x0025))->getU16());
    wbCoeffs[2] = static_cast<float>(
        raw->getEntry(static_cast<TiffTag>(0x0026))->getU16());
    mRaw->metadata.wbCoeffs = wbCoeffs;
  } else if (raw->hasEntry(static_cast<TiffTag>(0x0011)) &&
             raw->hasEntry(static_cast<TiffTag>(0x0012))) {
    std::array<float, 4> wbCoeffs = {};
    wbCoeffs[0] = static_cast<float>(
        raw->getEntry(static_cast<TiffTag>(0x0011))->getU16());
    wbCoeffs[1] = 256.0F;
    wbCoeffs[2] = static_cast<float>(
        raw->getEntry(static_cast<TiffTag>(0x0012))->getU16());
    mRaw->metadata.wbCoeffs = wbCoeffs;
  }
}

std::string Rw2Decoder::guessMode() const {
  float ratio = 3.0F / 2.0F; // Default

  if (!mRaw->isAllocated())
    return "";

  ratio = static_cast<float>(mRaw->dim.x) / static_cast<float>(mRaw->dim.y);

  float min_diff = fabs(ratio - (16.0F / 9.0F));
  std::string closest_match = "16:9";

  float t = fabs(ratio - (3.0F / 2.0F));
  if (t < min_diff) {
    closest_match = "3:2";
    min_diff = t;
  }

  t = fabs(ratio - (4.0F / 3.0F));
  if (t < min_diff) {
    closest_match = "4:3";
    min_diff = t;
  }

  t = fabs(ratio - 1.0F);
  if (t < min_diff) {
    closest_match = "1:1";
    min_diff = t;
  }
  writeLog(DEBUG_PRIO::EXTRA, "Mode guess: '%s'", closest_match.c_str());
  return closest_match;
}

rawspeed::iRectangle2D Rw2Decoder::getDefaultCrop() {
  if (const TiffIFD* raw = getRaw();
      raw->hasEntry(TiffTag::PANASONIC_SENSORLEFTBORDER) &&
      raw->hasEntry(TiffTag::PANASONIC_SENSORTOPBORDER) &&
      raw->hasEntry(TiffTag::PANASONIC_SENSORRIGHTBORDER) &&
      raw->hasEntry(TiffTag::PANASONIC_SENSORBOTTOMBORDER)) {
    const uint16_t leftBorder =
        raw->getEntry(TiffTag::PANASONIC_SENSORLEFTBORDER)->getU16();
    const uint16_t topBorder =
        raw->getEntry(TiffTag::PANASONIC_SENSORTOPBORDER)->getU16();
    const uint16_t rightBorder =
        raw->getEntry(TiffTag::PANASONIC_SENSORRIGHTBORDER)->getU16();
    const uint16_t bottomBorder =
        raw->getEntry(TiffTag::PANASONIC_SENSORBOTTOMBORDER)->getU16();
    const uint16_t width = rightBorder - leftBorder;
    const uint16_t height = bottomBorder - topBorder;
    return {leftBorder, topBorder, width, height};
  }
  ThrowRDE("Cannot figure out vendor crop. Required entries were not found: "
           "%X, %X, %X, %X",
           static_cast<unsigned int>(TiffTag::PANASONIC_SENSORLEFTBORDER),
           static_cast<unsigned int>(TiffTag::PANASONIC_SENSORTOPBORDER),
           static_cast<unsigned int>(TiffTag::PANASONIC_SENSORRIGHTBORDER),
           static_cast<unsigned int>(TiffTag::PANASONIC_SENSORBOTTOMBORDER));
}

} // namespace rawspeed
