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

#include "decoders/ArwDecoder.h"
#include "adt/Array1DRef.h"
#include "adt/Array2DRef.h"
#include "adt/Casts.h"
#include "adt/Invariant.h"
#include "adt/NORangesSet.h"
#include "adt/Point.h"
#include "bitstreams/BitStreams.h"
#include "common/Common.h"
#include "common/RawImage.h"
#include "common/RawspeedException.h"
#include "decoders/RawDecoderException.h"
#include "decompressors/LJpegDecoder.h"
#include "decompressors/SonyArw1Decompressor.h"
#include "decompressors/SonyArw2Decompressor.h"
#include "decompressors/UncompressedDecompressor.h"
#include "io/Buffer.h"
#include "io/ByteStream.h"
#include "io/Endianness.h"
#include "io/IOException.h"
#include "metadata/Camera.h"
#include "metadata/ColorFilterArray.h"
#include "tiff/TiffEntry.h"
#include "tiff/TiffIFD.h"
#include "tiff/TiffTag.h"
#include <array>
#include <cassert>
#include <cinttypes>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

using std::vector;

namespace rawspeed {

bool ArwDecoder::isAppropriateDecoder(const TiffRootIFD* rootIFD,
                                      [[maybe_unused]] Buffer file) {
  const auto id = rootIFD->getID();
  const std::string& make = id.make;

  // FIXME: magic

  return make == "SONY";
}

RawImage ArwDecoder::decodeSRF() {
  const TiffIFD* raw = mRootIFD->getIFDWithTag(TiffTag::IMAGEWIDTH);

  uint32_t width = raw->getEntry(TiffTag::IMAGEWIDTH)->getU32();
  uint32_t height = raw->getEntry(TiffTag::IMAGELENGTH)->getU32();

  if (width == 0 || height == 0 || width > 3360 || height > 2460)
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", width, height);

  uint32_t len = width * height * 2;

  // Constants taken from dcraw
  uint32_t off = 862144;
  uint32_t key_off = 200896;
  uint32_t head_off = 164600;

  // Replicate the dcraw contortions to get the "decryption" key
  uint8_t offset = mFile[key_off];
  const Buffer keyData = mFile.getSubView(key_off + 4 * offset, 4);
  uint32_t key = getU32BE(keyData.begin());

  static const size_t head_size = 40;
  const auto head_orig =
      mFile.getSubView(head_off, head_size).getAsArray1DRef();
  vector<uint8_t> head(head_size);
  SonyDecrypt(head_orig, {head.data(), head_size}, head_size / 4, key);
  for (int i = 26; i > 22; i--)
    key = key << 8 | head[i - 1];

  // "Decrypt" the whole image buffer
  const auto image_data = mFile.getSubView(off, len).getAsArray1DRef();
  std::vector<uint8_t> image_decoded(len);
  SonyDecrypt(image_data, {image_decoded.data(), implicit_cast<int>(len)},
              len / 4, key);

  Buffer di(image_decoded.data(), len);

  // And now decode as a normal 16bit raw
  mRaw->dim = iPoint2D(width, height);

  UncompressedDecompressor u(
      ByteStream(DataBuffer(di.getSubView(0, len), Endianness::little)), mRaw,
      iRectangle2D({0, 0}, iPoint2D(width, height)), 2 * width, 16,
      BitOrder::MSB);
  mRaw->createData();
  u.readUncompressedRaw();

  return mRaw;
}

RawImage ArwDecoder::decodeTransitionalArw() {
  if (const TiffEntry* model = mRootIFD->getEntryRecursive(TiffTag::MODEL);
      model && model->getString() == "DSLR-A100") {
    // We've caught the elusive A100 in the wild, a transitional format
    // between the simple sanity of the MRW custom format and the wordly
    // wonderfullness of the Tiff-based ARW format, let's shoot from the hip
    const TiffIFD* raw = mRootIFD->getIFDWithTag(TiffTag::SUBIFDS);
    uint32_t off = raw->getEntry(TiffTag::SUBIFDS)->getU32();
    uint32_t width = 3881;
    uint32_t height = 2608;

    mRaw->dim = iPoint2D(width, height);

    ByteStream input(DataBuffer(mFile.getSubView(off), Endianness::little));
    SonyArw1Decompressor a(mRaw);
    mRaw->createData();
    a.decompress(input);

    return mRaw;
  }

  if (hints.contains("srf_format"))
    return decodeSRF();

  ThrowRDE("No image data found");
}

std::vector<uint16_t> ArwDecoder::decodeCurve(const TiffIFD* raw) {
  std::vector<uint16_t> curve(0x4001);
  const TiffEntry* c = raw->getEntry(TiffTag::SONYCURVE);
  std::array<uint32_t, 6> sony_curve = {{0, 0, 0, 0, 0, 4095}};

  for (uint32_t i = 0; i < 4; i++)
    sony_curve[i + 1] = (c->getU16(i) >> 2) & 0xfff;

  for (uint32_t i = 0; i < 0x4001; i++)
    curve[i] = implicit_cast<uint16_t>(i);

  for (uint32_t i = 0; i < 5; i++)
    for (uint32_t j = sony_curve[i] + 1; j <= sony_curve[i + 1]; j++)
      curve[j] = implicit_cast<uint16_t>(curve[j - 1] + (1 << i));

  return curve;
}

RawImage ArwDecoder::decodeRawInternal() {
  vector<const TiffIFD*> data = mRootIFD->getIFDsWithTag(TiffTag::STRIPOFFSETS);

  if (data.empty())
    return decodeTransitionalArw();

  const TiffIFD* raw = data[0];
  int compression = raw->getEntry(TiffTag::COMPRESSION)->getU32();
  if (1 == compression) {
    DecodeUncompressed(raw);
    return mRaw;
  }

  if (7 == compression) {
    DecodeLJpeg(raw);
    // cropping of lossless compressed L files already done in Ljpeg decoder
    applyCrop = false;
    return mRaw;
  }

  if (32767 != compression)
    ThrowRDE("Unsupported compression %i", compression);

  const TiffEntry* offsets = raw->getEntry(TiffTag::STRIPOFFSETS);
  const TiffEntry* counts = raw->getEntry(TiffTag::STRIPBYTECOUNTS);

  if (offsets->count != 1) {
    ThrowRDE("Multiple Strips found: %u", offsets->count);
  }
  if (counts->count != offsets->count) {
    ThrowRDE(
        "Byte count number does not match strip size: count:%u, strips:%u ",
        counts->count, offsets->count);
  }
  uint32_t width = raw->getEntry(TiffTag::IMAGEWIDTH)->getU32();
  uint32_t height = raw->getEntry(TiffTag::IMAGELENGTH)->getU32();
  uint32_t bitPerPixel = raw->getEntry(TiffTag::BITSPERSAMPLE)->getU32();

  switch (bitPerPixel) {
  case 8:
  case 12:
  case 14:
    break;
  default:
    ThrowRDE("Unexpected bits per pixel: %u", bitPerPixel);
  }

  // Sony E-550 marks compressed 8bpp ARW with 12 bit per pixel
  // this makes the compression detect it as a ARW v1.
  // This camera has however another MAKER entry, so we MAY be able
  // to detect it this way in the future.
  data = mRootIFD->getIFDsWithTag(TiffTag::MAKE);
  if (data.size() > 1) {
    for (auto& i : data) {
      std::string make = i->getEntry(TiffTag::MAKE)->getString();
      /* Check for maker "SONY" without spaces */
      if (make == "SONY")
        bitPerPixel = 8;
    }
  }

  if (width == 0 || height == 0 || height % 2 != 0 || width > 9600 ||
      height > 6376)
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", width, height);

  bool arw1 = uint64_t(counts->getU32()) * 8 != width * height * bitPerPixel;
  if (arw1)
    height += 8;

  mRaw->dim = iPoint2D(width, height);

  std::vector<uint16_t> curve = decodeCurve(raw);

  RawImageCurveGuard curveHandler(&mRaw, curve, uncorrectedRawValues);

  uint32_t c2 = counts->getU32();
  uint32_t off = offsets->getU32();

  if (!mFile.isValid(off))
    ThrowRDE("Data offset after EOF, file probably truncated");

  if (!mFile.isValid(off, c2))
    c2 = mFile.getSize() - off;

  ByteStream input(DataBuffer(mFile.getSubView(off, c2), Endianness::little));

  if (arw1) {
    SonyArw1Decompressor a(mRaw);
    mRaw->createData();
    a.decompress(input);
    mShiftDownScaleForExif = 2;
  } else
    DecodeARW2(input, width, height, bitPerPixel);

  if (bitPerPixel == 12)
    mShiftDownScaleForExif = 2;

  return mRaw;
}

void ArwDecoder::DecodeUncompressed(const TiffIFD* raw) const {
  uint32_t width = raw->getEntry(TiffTag::IMAGEWIDTH)->getU32();
  uint32_t height = raw->getEntry(TiffTag::IMAGELENGTH)->getU32();
  uint32_t off = raw->getEntry(TiffTag::STRIPOFFSETS)->getU32();
  uint32_t c2 = raw->getEntry(TiffTag::STRIPBYTECOUNTS)->getU32();

  mRaw->dim = iPoint2D(width, height);

  if (width == 0 || height == 0 || width > 9600 || height > 6376)
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", width, height);

  if (c2 == 0)
    ThrowRDE("Strip is empty, nothing to decode!");

  const Buffer buf(mFile.getSubView(off, c2));

  if (hints.contains("sr2_format")) {
    UncompressedDecompressor u(ByteStream(DataBuffer(buf, Endianness::little)),
                               mRaw,
                               iRectangle2D({0, 0}, iPoint2D(width, height)),
                               2 * width, 16, BitOrder::MSB);
    mRaw->createData();
    u.readUncompressedRaw();
  } else {
    UncompressedDecompressor u(ByteStream(DataBuffer(buf, Endianness::little)),
                               mRaw,
                               iRectangle2D({0, 0}, iPoint2D(width, height)),
                               2 * width, 16, BitOrder::LSB);
    mRaw->createData();
    u.readUncompressedRaw();
  }
}

void ArwDecoder::DecodeLJpeg(const TiffIFD* raw) {
  uint32_t width = raw->getEntry(TiffTag::IMAGEWIDTH)->getU32();
  uint32_t height = raw->getEntry(TiffTag::IMAGELENGTH)->getU32();
  uint32_t bitPerPixel = raw->getEntry(TiffTag::BITSPERSAMPLE)->getU32();
  uint32_t photometric =
      raw->getEntry(TiffTag::PHOTOMETRICINTERPRETATION)->getU32();

  if (photometric != 32803)
    ThrowRDE("Unsupported photometric interpretation: %u", photometric);

  switch (bitPerPixel) {
  case 8:
  case 12:
  case 14:
    break;
  default:
    ThrowRDE("Unexpected bits per pixel: %u", bitPerPixel);
  }

  if (width == 0 || height == 0 || width % 2 != 0 || height % 2 != 0 ||
      width > 9728 || height > 6656)
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", width, height);

  mRaw->dim = iPoint2D(width, height);

  auto tilew = uint64_t(raw->getEntry(TiffTag::TILEWIDTH)->getU32());
  uint32_t tileh = raw->getEntry(TiffTag::TILELENGTH)->getU32();

  if (tilew <= 0 || tileh <= 0 || tileh % 2 != 0)
    ThrowRDE("Invalid tile size: (%" PRIu64 ", %u)", tilew, tileh);

  assert(tilew > 0);
  const auto tilesX =
      implicit_cast<uint32_t>(roundUpDivisionSafe(mRaw->dim.x, tilew));
  if (!tilesX)
    ThrowRDE("Zero tiles horizontally");

  assert(tileh > 0);
  const auto tilesY =
      implicit_cast<uint32_t>(roundUpDivisionSafe(mRaw->dim.y, tileh));
  if (!tilesY)
    ThrowRDE("Zero tiles vertically");

  // Math thoughs: if we know that the total size is 100, while tile size is 11,
  // we end up with 9 full tiles, and 1 partial tile (10 total).
  //
  // BUT! If we know that the total size is 100, and we have same 10 tiles,
  // we'd naively guess that each tile's size is 10, and not 11...

  const TiffEntry* offsets = raw->getEntry(TiffTag::TILEOFFSETS);
  const TiffEntry* counts = raw->getEntry(TiffTag::TILEBYTECOUNTS);
  if (offsets->count != counts->count) {
    ThrowRDE("Tile count mismatch: offsets:%u count:%u", offsets->count,
             counts->count);
  }

  // tilesX * tilesY may overflow, but division is fine, so let's do that.
  if ((offsets->count / tilesX != tilesY || (offsets->count % tilesX != 0)) ||
      (offsets->count / tilesY != tilesX || (offsets->count % tilesY != 0))) {
    ThrowRDE("Tile X/Y count mismatch: total:%u X:%u, Y:%u", offsets->count,
             tilesX, tilesY);
  }

  NORangesSet<Buffer> tilesLegality;
  for (int tile = 0U; tile < implicit_cast<int>(offsets->count); tile++) {
    const uint32_t offset = offsets->getU32(tile);
    const uint32_t length = counts->getU32(tile);
    if (!tilesLegality.insert(mFile.getSubView(offset, length)))
      ThrowRDE("Two tiles overlap. Raw corrupt!");
  }

  mRaw->createData();
#ifdef HAVE_OPENMP
#pragma omp parallel for schedule(static) default(none)                        \
    shared(offsets, counts) firstprivate(tilesX, tilew, tileh)
#endif
  for (int tile = 0U; tile < static_cast<int>(offsets->count); tile++) {
    try {
      const uint32_t tileX = tile % tilesX;
      const uint32_t tileY = tile / tilesX;
      const uint32_t offset = offsets->getU32(tile);
      const uint32_t length = counts->getU32(tile);

      LJpegDecoder decoder(
          ByteStream(
              DataBuffer(mFile.getSubView(offset, length), Endianness::little)),
          mRaw);
      auto offsetX = implicit_cast<uint32_t>(tileX * tilew);
      auto offsetY = tileY * tileh;
      auto tileWidth = implicit_cast<uint32_t>(tilew);
      auto tileHeight = tileh;
      auto maxDim = iPoint2D{implicit_cast<int>(tileWidth),
                             implicit_cast<int>(tileHeight)};
      decoder.decode(offsetX, offsetY, tileWidth, tileHeight, maxDim,
                     /*fixDng16Bug=*/false);
    } catch (const RawDecoderException& err) {
      mRaw->setError(err.what());
    } catch (const IOException& err) {
      mRaw->setError(err.what());
    } catch (...) {
      // We should not get any other exception type here.
      __builtin_unreachable();
    }
  }

  std::string firstErr;
  if (mRaw->isTooManyErrors(1, &firstErr)) {
    ThrowRDE("Too many errors encountered. Giving up. First Error:\n%s",
             firstErr.c_str());
  }

  const TiffEntry* size_entry = raw->getEntry(TiffTag::SONYRAWIMAGESIZE);
  iRectangle2D crop(0, 0, size_entry->getU32(0), size_entry->getU32(1));
  mRaw->subFrame(crop);
}

void ArwDecoder::DecodeARW2(ByteStream input, uint32_t w, uint32_t h,
                            uint32_t bpp) {

  if (bpp == 8) {
    SonyArw2Decompressor a2(mRaw, input);
    mRaw->createData();
    a2.decompress();
    return;
  } // End bpp = 8

  if (bpp == 12) {
    input.setByteOrder(Endianness::little);
    UncompressedDecompressor u(input, mRaw,
                               iRectangle2D({0, 0}, iPoint2D(w, h)),
                               bpp * w / 8, bpp, BitOrder::LSB);
    mRaw->createData();
    u.readUncompressedRaw();

    // Shift scales, since black and white are the same as compressed precision
    mShiftDownScale = 2;
    return;
  }
  ThrowRDE("Unsupported bit depth");
}

void ArwDecoder::ParseA100WB() const {
  if (!mRootIFD->hasEntryRecursive(TiffTag::DNGPRIVATEDATA))
    return;

  // only contains the offset, not the length!
  const TiffEntry* priv = mRootIFD->getEntryRecursive(TiffTag::DNGPRIVATEDATA);
  ByteStream bs = priv->getData();
  bs.setByteOrder(Endianness::little);
  const uint32_t off = bs.getU32();

  bs = ByteStream(DataBuffer(mFile.getSubView(off), Endianness::little));

  // MRW style, see MrwDecoder

  bs.setByteOrder(Endianness::big);
  uint32_t tag = bs.getU32();
  if (0x4D5249 != tag) // MRI
    ThrowRDE("Can not parse DNGPRIVATEDATA, invalid tag (0x%x).", tag);

  bs.setByteOrder(Endianness::little);
  uint32_t len = bs.getU32();

  bs = bs.getSubStream(bs.getPosition(), len);

  while (bs.getRemainSize() > 0) {
    bs.setByteOrder(Endianness::big);
    tag = bs.getU32();
    bs.setByteOrder(Endianness::little);
    len = bs.getU32();
    (void)bs.check(len);
    if (!len)
      ThrowRDE("Found entry of zero length, corrupt.");

    if (0x574247 != tag) { // WBG
      // not the tag we are interested in, skip
      bs.skipBytes(len);
      continue;
    }

    bs.skipBytes(4);

    bs.setByteOrder(Endianness::little);
    std::array<uint16_t, 4> tmp;
    for (auto& coeff : tmp)
      coeff = bs.getU16();

    mRaw->metadata.wbCoeffs[0] = static_cast<float>(tmp[0]);
    mRaw->metadata.wbCoeffs[1] = static_cast<float>(tmp[1]);
    mRaw->metadata.wbCoeffs[2] = static_cast<float>(tmp[3]);

    // only need this one block, no need to process any further
    break;
  }
}

void ArwDecoder::decodeMetaDataInternal(const CameraMetaData* meta) {
  // Default
  int iso = 0;

  mRaw->cfa.setCFA(iPoint2D(2, 2), CFAColor::RED, CFAColor::GREEN,
                   CFAColor::GREEN, CFAColor::BLUE);

  if (mRootIFD->hasEntryRecursive(TiffTag::ISOSPEEDRATINGS))
    iso = mRootIFD->getEntryRecursive(TiffTag::ISOSPEEDRATINGS)->getU32();

  auto id = mRootIFD->getID();

  setMetaData(meta, id, "", iso);
  if (mRaw->whitePoint)
    mRaw->whitePoint = *mRaw->whitePoint >> mShiftDownScale;
  mRaw->blackLevel >>= mShiftDownScale;

  // Set the whitebalance
  try {
    if (id.model == "DSLR-A100") { // Handle the MRW style WB of the A100
      ParseA100WB();
    } else { // Everything else but the A100
      GetWB();
    }
  } catch (const RawspeedException& e) {
    mRaw->setError(e.what());
    // We caught an exception reading WB, just ignore it
  }
}

void ArwDecoder::SonyDecrypt(Array1DRef<const uint8_t> ibuf,
                             Array1DRef<uint8_t> obuf, int len, uint32_t key) {
  invariant(ibuf.size() == obuf.size());
  invariant(ibuf.size() == 4 * len);
  invariant(obuf.size() == 4 * len);

  if (0 == len)
    return;

  std::array<uint32_t, 128> pad;

  // Initialize the decryption pad from the key
  for (int p = 0; p < 4; p++)
    pad[p] = key = uint32_t(key * 48828125UL + 1UL);
  pad[3] = pad[3] << 1 | (pad[0] ^ pad[2]) >> 31;
  for (int p = 4; p < 127; p++)
    pad[p] = (pad[p - 4] ^ pad[p - 2]) << 1 | (pad[p - 3] ^ pad[p - 1]) >> 31;
  for (int p = 0; p < 127; p++)
    pad[p] = getU32BE(&pad[p]);

  int p = 127;
  // Decrypt the buffer in place using the pad
  for (int i = 0; i != len; ++i) {
    pad[p & 127] = pad[(p + 1) & 127] ^ pad[(p + 1 + 64) & 127];

    uint32_t pv = pad[p & 127];

    uint32_t bv;
    memcpy(&bv, ibuf.getBlock(4, i).begin(), sizeof(uint32_t));

    bv ^= pv;

    memcpy(obuf.getBlock(4, i).begin(), &bv, sizeof(uint32_t));

    p++;
  }
}

void ArwDecoder::GetWB() const {
  // Set the whitebalance for all the modern ARW formats (everything after A100)
  if (mRootIFD->hasEntryRecursive(TiffTag::DNGPRIVATEDATA)) {
    NORangesSet<Buffer> ifds_undecoded;

    const TiffEntry* priv =
        mRootIFD->getEntryRecursive(TiffTag::DNGPRIVATEDATA);
    TiffRootIFD makerNoteIFD(nullptr, &ifds_undecoded, priv->getRootIfdData(),
                             priv->getU32());

    const TiffEntry* sony_offset =
        makerNoteIFD.getEntryRecursive(TiffTag::SONYOFFSET);
    const TiffEntry* sony_length =
        makerNoteIFD.getEntryRecursive(TiffTag::SONYLENGTH);
    const TiffEntry* sony_key =
        makerNoteIFD.getEntryRecursive(TiffTag::SONYKEY);
    if (!sony_offset || !sony_length || !sony_key || sony_key->count != 4)
      ThrowRDE("couldn't find the correct metadata for WB decoding");

    assert(sony_offset != nullptr);
    uint32_t off = sony_offset->getU32();

    assert(sony_length != nullptr);
    // The Decryption is done in blocks of 4 bytes.
    auto len = implicit_cast<uint32_t>(roundDown(sony_length->getU32(), 4));
    if (!len)
      ThrowRDE("No buffer to decrypt?");

    assert(sony_key != nullptr);
    uint32_t key = getU32LE(sony_key->getData().getData(4));

    // "Decrypt" IFD
    const auto& ifd_crypt = priv->getRootIfdData();
    const auto EncryptedBuffer =
        ifd_crypt.getSubView(off, len).getAsArray1DRef();
    // We do have to prepend 'off' padding, because TIFF uses absolute offsets.
    const auto DecryptedBufferSize = off + EncryptedBuffer.size();
    std::vector<uint8_t> DecryptedBuffer(DecryptedBufferSize);

    SonyDecrypt(EncryptedBuffer,
                {&DecryptedBuffer[off], implicit_cast<int>(len)}, len / 4, key);

    NORangesSet<Buffer> ifds_decoded;
    Buffer decIFD(DecryptedBuffer.data(), DecryptedBufferSize);
    const Buffer Padding(decIFD.getSubView(0, off));
    // The Decrypted Root Ifd can not point to preceding padding buffer.
    ifds_decoded.insert(Padding);

    DataBuffer dbIDD(decIFD, priv->getRootIfdData().getByteOrder());
    TiffRootIFD encryptedIFD(nullptr, &ifds_decoded, dbIDD, off);

    if (encryptedIFD.hasEntry(TiffTag::SONYGRBGLEVELS)) {
      const TiffEntry* wb = encryptedIFD.getEntry(TiffTag::SONYGRBGLEVELS);
      if (wb->count != 4)
        ThrowRDE("WB has %d entries instead of 4", wb->count);
      mRaw->metadata.wbCoeffs[0] = wb->getFloat(1);
      mRaw->metadata.wbCoeffs[1] = wb->getFloat(0);
      mRaw->metadata.wbCoeffs[2] = wb->getFloat(2);
    } else if (encryptedIFD.hasEntry(TiffTag::SONYRGGBLEVELS)) {
      const TiffEntry* wb = encryptedIFD.getEntry(TiffTag::SONYRGGBLEVELS);
      if (wb->count != 4)
        ThrowRDE("WB has %d entries instead of 4", wb->count);
      mRaw->metadata.wbCoeffs[0] = wb->getFloat(0);
      mRaw->metadata.wbCoeffs[1] = wb->getFloat(1);
      mRaw->metadata.wbCoeffs[2] = wb->getFloat(3);
    }

    if (encryptedIFD.hasEntry(TiffTag::SONYBLACKLEVEL)) {
      const TiffEntry* bl = encryptedIFD.getEntry(TiffTag::SONYBLACKLEVEL);
      if (bl->count != 4)
        ThrowRDE("Black Level has %d entries instead of 4", bl->count);
      mRaw->blackLevelSeparate =
          Array2DRef(mRaw->blackLevelSeparateStorage.data(), 2, 2);
      auto blackLevelSeparate1D = *mRaw->blackLevelSeparate->getAsArray1DRef();
      for (int i = 0; i < 4; ++i)
        blackLevelSeparate1D(i) = bl->getU16(i) >> mShiftDownScaleForExif;
    }

    if (encryptedIFD.hasEntry(TiffTag::SONYWHITELEVEL)) {
      const TiffEntry* wl = encryptedIFD.getEntry(TiffTag::SONYWHITELEVEL);
      if (wl->count != 1 && wl->count != 3)
        ThrowRDE("White Level has %d entries instead of 1 or 3", wl->count);
      mRaw->whitePoint = wl->getU16(0) >> mShiftDownScaleForExif;
    }
  }
}

} // namespace rawspeed
