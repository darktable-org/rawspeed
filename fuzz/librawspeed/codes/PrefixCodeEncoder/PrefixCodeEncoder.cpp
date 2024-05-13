/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2024 Roman Lebedev

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

#include "adt/Array1DRef.h"
#include "adt/Casts.h"
#include "adt/Optional.h"
#include "adt/PartitioningOutputIterator.h"
#include "bitstreams/BitStreamerJPEG.h"
#include "bitstreams/BitStreamerMSB.h"
#include "bitstreams/BitStreamerMSB32.h"
#include "bitstreams/BitVacuumerJPEG.h"
#include "bitstreams/BitVacuumerMSB.h"
#include "bitstreams/BitVacuumerMSB32.h"
#include "codes/PrefixCodeDecoder.h"
#include "codes/PrefixCodeDecoder/Common.h"
#include "codes/PrefixCodeTreeDecoder.h"
#include "codes/PrefixCodeVectorDecoder.h"
#include "codes/PrefixCodeVectorEncoder.h"
#include "common/RawspeedException.h"
#include "io/Buffer.h"
#include "io/ByteStream.h"
#include "io/Endianness.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size);

namespace rawspeed {

namespace {

struct BitstreamFlavorMSB;
struct BitstreamFlavorMSB32;
struct BitstreamFlavorJPEG;

template <typename T> struct BitStreamRoundtripTypes final {};

template <> struct BitStreamRoundtripTypes<BitstreamFlavorMSB> final {
  using streamer = BitStreamerMSB;

  template <typename OutputIterator>
  using vacuumer = BitVacuumerMSB<OutputIterator>;
};

template <> struct BitStreamRoundtripTypes<BitstreamFlavorMSB32> final {
  using streamer = BitStreamerMSB32;

  template <typename OutputIterator>
  using vacuumer = BitVacuumerMSB32<OutputIterator>;
};

template <> struct BitStreamRoundtripTypes<BitstreamFlavorJPEG> final {
  using streamer = BitStreamerJPEG;

  template <typename OutputIterator>
  using vacuumer = BitVacuumerJPEG<OutputIterator>;
};

template <typename flavor, bool IsFullDecode, typename HT>
void workloop(Array1DRef<const uint8_t> inputSrc, const HT& ht) {
  PrefixCodeVectorEncoder<typename HT::Tag> encoder(ht.code);
  encoder.setup(IsFullDecode, ht.handleDNGBug16());

  using BitStreamer = typename BitStreamRoundtripTypes<flavor>::streamer;

  std::vector<uint8_t> inputRec;

  int numCodesEncoded = 0;
  {
    auto bsSrc = BitStreamer(inputSrc);

    auto bsInserter = PartitioningOutputIterator(std::back_inserter(inputRec));
    using BitVacuumer = typename BitStreamRoundtripTypes<
        flavor>::template vacuumer<decltype(bsInserter)>;
    auto bv = BitVacuumer(bsInserter);

    bsSrc.fill(32);
    while (bsSrc.getInputPosition() <= inputSrc.size()) {
      Optional<int> v;

      try {
        v = ht.template decode<BitStreamer, IsFullDecode>(bsSrc);
      } catch (...) {
        break;
      }

      try {
        encoder.template encode<BitVacuumer, IsFullDecode>(bv, *v);
      } catch (...) {
        __builtin_unreachable();
      }
      ++numCodesEncoded;
    }
  }

  if (constexpr int MinSize = BitStreamer::Traits::MaxProcessBytes;
      inputRec.size() < MinSize)
    inputRec.resize(MinSize, uint8_t(0));

  try {
    auto bsSrc = BitStreamer(inputSrc);
    auto bsRec = BitStreamer(
        Array1DRef(inputRec.data(), implicit_cast<int>(inputRec.size())));
    for (int i = 0; i != numCodesEncoded; ++i) {
      const auto vSrc = ht.template decode<BitStreamer, IsFullDecode>(bsSrc);
      const auto vRec = ht.template decode<BitStreamer, IsFullDecode>(bsRec);
      assert(vSrc == vRec);
      (void)vSrc;
      (void)vRec;
    }
  } catch (...) {
    __builtin_unreachable();
  }
}

template <typename flavor, typename HT>
void checkPump(Array1DRef<const uint8_t> input, const HT& ht) {
  if (ht.isFullDecode())
    workloop<flavor, /*IsFullDecode=*/true>(input, ht);
  else
    workloop<flavor, /*IsFullDecode=*/false>(input, ht);
}

template <typename HT> void checkDecoder(ByteStream bs) {
  const auto ht = createPrefixCodeDecoder<HT>(bs);

  // Which bit stream flavor should we use?
  const auto flavor = bs.getByte();
  const auto input = bs.peekRemainingBuffer().getAsArray1DRef();
  switch (flavor) {
  case 0:
    checkPump<BitstreamFlavorMSB>(input, ht);
    break;
  case 1:
    checkPump<BitstreamFlavorMSB32>(input, ht);
    break;
  case 2:
    checkPump<BitstreamFlavorJPEG>(input, ht);
    break;
  default:
    ThrowRSE("Unknown bit pump");
  }
}

template <typename CodeTag> void checkFlavour(ByteStream bs) {
  // Which decoder implementation should we use?
  const auto decoderImpl = bs.getByte();
  switch (decoderImpl) {
  case 0:
    checkDecoder<PrefixCodeTreeDecoder<CodeTag>>(bs);
    break;
  case 1:
    checkDecoder<PrefixCodeVectorDecoder<CodeTag>>(bs);
    break;
  case 2:
    checkDecoder<PrefixCodeLookupDecoder<CodeTag>>(bs);
    break;
  case 3: {
    // Which backing decoder implementation should we use?
    const auto backingDecoderImpl = bs.getByte();
    switch (backingDecoderImpl) {
    case 0:
      checkDecoder<
          PrefixCodeLUTDecoder<CodeTag, PrefixCodeTreeDecoder<CodeTag>>>(bs);
      break;
    case 1:
      checkDecoder<
          PrefixCodeLUTDecoder<CodeTag, PrefixCodeVectorDecoder<CodeTag>>>(bs);
      break;
    case 2:
      checkDecoder<
          PrefixCodeLUTDecoder<CodeTag, PrefixCodeLookupDecoder<CodeTag>>>(bs);
      break;
    default:
      ThrowRSE("Unknown decoder");
    }
    break;
  }
  default:
    ThrowRSE("Unknown decoder");
  }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size) {
  assert(Data);

  try {
    const Buffer b(Data, implicit_cast<Buffer::size_type>(Size));
    const DataBuffer db(b, Endianness::little);
    ByteStream bs(db);

    // Which flavor?
    switch (bs.getByte()) {
    case 0:
      checkFlavour<BaselineCodeTag>(bs);
      break;
    case 1:
      checkFlavour<VC5CodeTag>(bs);
      break;
    default:
      ThrowRSE("Unknown flavor");
    }
  } catch (const RawspeedException&) {
    return 0;
  }

  return 0;
}

} // namespace

} // namespace rawspeed
