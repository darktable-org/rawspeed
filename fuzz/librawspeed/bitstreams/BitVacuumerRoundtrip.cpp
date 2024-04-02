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

#include "rawspeedconfig.h"
#include "adt/Bit.h"
#include "adt/Array1DRef.h"
#include "adt/Casts.h"
#include "adt/Invariant.h"
#include "adt/PartitioningOutputIterator.h"
#include "bitstreams/BitStreamer.h"
#include "bitstreams/BitStreamerJPEG.h"
#include "bitstreams/BitStreamerLSB.h"
#include "bitstreams/BitStreamerMSB.h"
#include "bitstreams/BitStreamerMSB16.h"
#include "bitstreams/BitStreamerMSB32.h"
#include "bitstreams/BitVacuumerJPEG.h"
#include "bitstreams/BitVacuumerLSB.h"
#include "bitstreams/BitVacuumerMSB.h"
#include "bitstreams/BitVacuumerMSB16.h"
#include "bitstreams/BitVacuumerMSB32.h"
#include "common/RawspeedException.h"
#include "io/Buffer.h"
#include "io/ByteStream.h"
#include "io/Endianness.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <utility>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size);

namespace rawspeed {
namespace {

struct BitstreamFlavorLSB;
struct BitstreamFlavorMSB;
struct BitstreamFlavorMSB16;
struct BitstreamFlavorMSB32;
struct BitstreamFlavorJPEG;

template <typename T> struct BitStreamRoundtripTypes final {};

template <> struct BitStreamRoundtripTypes<BitstreamFlavorLSB> final {
  using streamer = BitStreamerLSB;

  template <typename OutputIterator>
  using vacuumer = BitVacuumerLSB<OutputIterator>;
};

template <> struct BitStreamRoundtripTypes<BitstreamFlavorMSB> final {
  using streamer = BitStreamerMSB;

  template <typename OutputIterator>
  using vacuumer = BitVacuumerMSB<OutputIterator>;
};

template <> struct BitStreamRoundtripTypes<BitstreamFlavorMSB16> final {
  using streamer = BitStreamerMSB16;

  template <typename OutputIterator>
  using vacuumer = BitVacuumerMSB16<OutputIterator>;
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

class InputWrapper final {
  const ByteStream bitLengths;
  const ByteStream bitVals;

public:
  InputWrapper() = delete;

  InputWrapper(const InputWrapper&) = delete;
  InputWrapper(InputWrapper&&) = delete;
  InputWrapper& operator=(const InputWrapper&) = delete;
  InputWrapper& operator=(InputWrapper&&) = delete;

  InputWrapper(ByteStream bitLengths_, ByteStream bitVals_)
      : bitLengths(bitLengths_), bitVals(bitVals_) {
    invariant(size() >= 0);
  }

  [[nodiscard]] int size() const RAWSPEED_READNONE {
    invariant(bitVals.getSize() == 4 * bitLengths.getSize());
    return bitLengths.getSize();
  }

  [[nodiscard]] std::pair<uint32_t, int>
  operator[](int i) const RAWSPEED_READNONE {
    invariant(i >= 0);
    invariant(i < size());

    auto len = implicit_cast<int>(bitLengths.peekByte(i) % 33); // 0-32 bits
    uint32_t val = extractLowBitsSafe(bitVals.peekU32(i), len);

    return {val, len};
  }
};

template <typename flavor>
std::vector<uint8_t> produceBitstream(const InputWrapper& w) {
  std::vector<uint8_t> bitstream;

  {
    auto bsInserter = PartitioningOutputIterator(std::back_inserter(bitstream));
    using BitVacuumer = typename BitStreamRoundtripTypes<
        flavor>::template vacuumer<decltype(bsInserter)>;
    auto bv = BitVacuumer(bsInserter);

    for (int i = 0; i != w.size(); ++i) {
      const auto [val, len] = w[i];
      bv.put(val, len);
    }
  }

  using BitStreamer = typename BitStreamRoundtripTypes<flavor>::streamer;
  if (constexpr int MinSize = BitStreamerTraits<BitStreamer>::MaxProcessBytes;
      bitstream.size() < MinSize)
    bitstream.resize(MinSize, uint8_t(0));

  return bitstream;
}

template <typename flavor>
void reparseBitstream(Array1DRef<const uint8_t> input, const InputWrapper& w) {
  using BitStreamer = typename BitStreamRoundtripTypes<flavor>::streamer;
  auto bs = BitStreamer(input);
  for (int i = 0; i != w.size(); ++i) {
    const auto [expectedVal, len] = w[i];
    bs.fill(32);
    const auto actualVal = len != 0 ? bs.getBitsNoFill(len) : 0;
    invariant(actualVal == expectedVal);
  }
}

template <typename flavor> void checkFlavourImpl(const InputWrapper& w) {
  const std::vector<uint8_t> bitstream = produceBitstream<flavor>(w);
  const auto input =
      Array1DRef(bitstream.data(), implicit_cast<int>(bitstream.size()));
  reparseBitstream<flavor>(input, w);
}

template <typename flavor> void checkFlavour(const InputWrapper& w) {
  try {
    checkFlavourImpl<flavor>(w);
  } catch (const RawspeedException&) {
    assert(false && "Unexpected exception in `checkFlavourImpl()`.");
  }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size) {
  assert(Data);

  try {
    const Buffer b(Data, implicit_cast<Buffer::size_type>(Size));
    const DataBuffer db(b, Endianness::little);
    ByteStream bs(db);

    const auto flavor = bs.getByte();

    const auto numValues = bs.getU32();
    const auto bitLengths = bs.getStream(numValues, sizeof(uint8_t));
    const auto bitVals = bs.getStream(numValues, sizeof(uint32_t));
    const InputWrapper w(bitLengths, bitVals);

    // Which flavor?
    switch (flavor) {
    case 0:
      checkFlavour<BitstreamFlavorLSB>(w);
      return 0;
    case 1:
      checkFlavour<BitstreamFlavorMSB>(w);
      return 0;
    case 2:
      checkFlavour<BitstreamFlavorMSB16>(w);
      return 0;
    case 3:
      checkFlavour<BitstreamFlavorMSB32>(w);
      return 0;
    case 4:
      checkFlavour<BitstreamFlavorJPEG>(w);
      return 0;
    default:
      ThrowRSE("Unknown flavor");
    }
  } catch (const RawspeedException&) {
    return 0;
  }

  __builtin_unreachable();
}

} // namespace

} // namespace rawspeed
