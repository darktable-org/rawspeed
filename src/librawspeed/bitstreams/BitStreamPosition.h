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

#pragma once

#include "adt/Casts.h"
#include "adt/Invariant.h"
#include "bitstreams/BitStreams.h"
#include "common/Common.h"

namespace rawspeed {

template <BitOrder bo> struct BitStreamTraits;

template <BitOrder bo> struct BitStreamPosition {
  int pos;
  int fillLevel;
};

template <BitOrder bo> struct ByteStreamPosition {
  int bytePos;
  int numBitsToSkip;
};

template <BitOrder bo>
  requires BitStreamTraits<bo>::FixedSizeChunks
ByteStreamPosition<bo> getAsByteStreamPosition(BitStreamPosition<bo> state) {
  const int MinByteStepMultiple = BitStreamTraits<bo>::MinLoadStepByteMultiple;

  invariant(state.pos >= 0);
  invariant(state.pos % MinByteStepMultiple == 0);
  invariant(state.fillLevel >= 0);

  auto numBytesToBacktrack = implicit_cast<int>(
      MinByteStepMultiple *
      roundUpDivision(state.fillLevel, CHAR_BIT * MinByteStepMultiple));
  invariant(numBytesToBacktrack >= 0);
  invariant(numBytesToBacktrack <= state.pos);
  invariant(numBytesToBacktrack % MinByteStepMultiple == 0);

  auto numBitsToBacktrack = CHAR_BIT * numBytesToBacktrack;
  invariant(numBitsToBacktrack >= 0);

  ByteStreamPosition<bo> res;
  invariant(state.pos >= numBytesToBacktrack);
  res.bytePos = state.pos - numBytesToBacktrack;
  invariant(numBitsToBacktrack >= state.fillLevel);
  res.numBitsToSkip = numBitsToBacktrack - state.fillLevel;
  invariant(res.numBitsToSkip >= 0);
  invariant(res.numBitsToSkip < CHAR_BIT * MinByteStepMultiple);

  invariant(res.bytePos >= 0);
  invariant(res.bytePos <= state.pos);
  invariant(res.bytePos % MinByteStepMultiple == 0);
  invariant(res.numBitsToSkip >= 0);
  return res;
}

} // namespace rawspeed
