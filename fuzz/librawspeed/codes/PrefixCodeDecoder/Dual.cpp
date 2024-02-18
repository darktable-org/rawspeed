/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2017-2023 Roman Lebedev

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
#ifndef IMPL0
#error IMPL0 must be defined to one of rawspeeds huffman table implementations
#endif
#ifndef IMPL1
#error IMPL1 must be defined to one of rawspeeds huffman table implementations
#endif

#include "bitstreams/BitStreamerJPEG.h"
#include "bitstreams/BitStreamerMSB.h"
#include "bitstreams/BitStreamerMSB32.h"
#include "codes/PrefixCodeDecoder.h" // IWYU pragma: keep
#include "codes/PrefixCodeDecoder/Common.h"
#include "codes/PrefixCodeLUTDecoder.h"    // IWYU pragma: keep
#include "codes/PrefixCodeLookupDecoder.h" // IWYU pragma: keep
#include "codes/PrefixCodeTreeDecoder.h"   // IWYU pragma: keep
#include "codes/PrefixCodeVectorDecoder.h" // IWYU pragma: keep
#include "io/Buffer.h"
#include "io/ByteStream.h"
#include "io/Endianness.h"
#include "io/IOException.h"
#include <cassert>
#include <cstdint>
#include <cstdio>

namespace rawspeed {
struct BaselineCodeTag;
struct VC5CodeTag;
} // namespace rawspeed

namespace {

template <typename Pump, bool IsFullDecode, typename HT0, typename HT1>
void workloop(rawspeed::Array1DRef<const uint8_t> input, const HT0& ht0,
              const HT1& ht1) {
  Pump bits0(input);
  Pump bits1(input);

  while (true) {
    int decoded0;
    int decoded1;

    bool failure0 = false;
    bool failure1 = false;

    try {
      decoded1 = ht1.template decode<decltype(bits1), IsFullDecode>(bits1);
    } catch (const rawspeed::IOException&) {
      // For now, let's ignore stream depleteon issues.
      throw;
    } catch (const rawspeed::RawspeedException&) {
      failure1 = true;
    }

    try {
      decoded0 = ht0.template decode<decltype(bits0), IsFullDecode>(bits0);
    } catch (const rawspeed::IOException&) {
      // For now, let's ignore stream depleteon issues.
      throw;
    } catch (const rawspeed::RawspeedException&) {
      failure0 = true;
    }

    // They both should either fail or succeed, else there is a bug.
    assert(failure0 == failure1);

    // If any failed, we can't continue.
    if (failure0 || failure1)
      ThrowRSE("Failure detected");

    (void)decoded0;
    (void)decoded1;

    // They both should have decoded the same value.
    assert(decoded0 == decoded1);
  }
}

template <typename Pump, typename HT0, typename HT1>
void checkPump(rawspeed::Array1DRef<const uint8_t> input, const HT0& ht0,
               const HT1& ht1) {
  assert(ht0.isFullDecode() == ht1.isFullDecode());
  if (ht0.isFullDecode())
    workloop<Pump, /*IsFullDecode=*/true>(input, ht0, ht1);
  else
    workloop<Pump, /*IsFullDecode=*/false>(input, ht0, ht1);
}

template <typename CodeTag> void checkFlavour(rawspeed::ByteStream bs) {
  rawspeed::ByteStream bs0 = bs;
  rawspeed::ByteStream bs1 = bs;

#ifndef BACKIMPL0
  const auto ht0 = createPrefixCodeDecoder<rawspeed::IMPL0<CodeTag>>(bs0);
#else
  const auto ht0 = createPrefixCodeDecoder<
      rawspeed::IMPL0<CodeTag, rawspeed::BACKIMPL0<CodeTag>>>(bs0);
#endif

#ifndef BACKIMPL1
  const auto ht1 = createPrefixCodeDecoder<rawspeed::IMPL1<CodeTag>>(bs1);
#else
  const auto ht1 = createPrefixCodeDecoder<
      rawspeed::IMPL1<CodeTag, rawspeed::BACKIMPL1<CodeTag>>>(bs1);
#endif

  // Which bit pump should we use?
  const int format0 = bs0.getByte();
  const int format1 = bs1.getByte();

  // should have consumed 16 bytes for n-codes-per-length, at *least* 1 byte
  // as code value, and a byte per 'fixDNGBug16'/'fullDecode' booleans
  assert(bs0.getPosition() == bs1.getPosition());

  assert(format0 == format1);
  (void)format1;

  const auto input = bs0.peekRemainingBuffer().getAsArray1DRef();

  assert(format0 == format1);
  switch (format0) {
  case 0:
    checkPump<rawspeed::BitStreamerMSB>(input, ht0, ht1);
    break;
  case 1:
    checkPump<rawspeed::BitStreamerMSB32>(input, ht0, ht1);
    break;
  case 2:
    checkPump<rawspeed::BitStreamerJPEG>(input, ht0, ht1);
    break;
  default:
    ThrowRSE("Unknown bit pump");
  }
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size);

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size) {
  assert(Data);

  try {
    const rawspeed::Buffer b(
        Data, rawspeed::implicit_cast<rawspeed::Buffer::size_type>(Size));
    const rawspeed::DataBuffer db(b, rawspeed::Endianness::little);
    rawspeed::ByteStream bs(db);

    // Which flavor?
    switch (bs.getByte()) {
    case 0:
      checkFlavour<rawspeed::BaselineCodeTag>(bs);
      break;
    case 1:
      checkFlavour<rawspeed::VC5CodeTag>(bs);
      break;
    default:
      ThrowRSE("Unknown flavor");
    }
  } catch (const rawspeed::RawspeedException&) {
    return 0;
  }

  __builtin_unreachable();
}
