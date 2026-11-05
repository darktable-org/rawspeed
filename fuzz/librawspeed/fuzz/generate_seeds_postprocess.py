#!/usr/bin/env python3
"""Deterministic seeds for the new RawImagePostProcessFuzzer (option-matrix
fuzzer over RawImage post-processing: subFrame, bad pixels, black/white
levels + black areas, linearization table, UINT16/F32).

Input layout (little-endian, see fuzz/librawspeed/common/RawImagePostProcess.cpp):
  u32 type(0=U16,1=F32), u32 cpp-1, u32 raw width, u32 raw height,
  u32 isCFA, CFA blob (i32 w, i32 h, w*h x u32 colors),
  pixel payload, then the option vector:
  u32 cropFlag, [u32 cx, u32 cy, u32 cw, u32 ch],
  u32 badpixFlag, [u32 count, count x (u32 x, u32 y), u32 mapFlag],
  u32 blackLevel, u32 whitePointFlag, [u32 whitePoint],
  u32 separateFlag, [4 x u32], u32 blackAreaCount, per-area 3 x u32,
  u32 tableFlag, [u32 dither, u32 len, len x u16].
"""

import argparse
import os
import struct


def lcg_bytes(seed, count):
    out = bytearray()
    x = seed & 0x7FFFFFFF or 1
    for _ in range(count):
        x = (x * 1103515245 + 12345) & 0x7FFFFFFF
        out.append((x >> 16) & 0xFF)
    return bytes(out)


def seed(name, out, typ, cpp, w, h, is_cfa, pixel_bytes, crop=None, badpix=None,
         black=0, white=None, separate=None, areas=(), table=None):
    b = bytearray()
    b += struct.pack("<2I", typ, cpp - 1)
    b += struct.pack("<2I", w - 16, h - 16)
    b += struct.pack("<I", is_cfa)
    b += struct.pack("<ii", 2, 2) + struct.pack("<4I", 0, 1, 1, 2)  # RGGB
    b += pixel_bytes
    if crop is None:
        b += struct.pack("<I", 0)
    else:
        b += struct.pack("<5I", 1, *crop)
    if badpix is None:
        b += struct.pack("<I", 0)
    else:
        count, coords, mapflag = badpix
        b += struct.pack("<II", 1, count)
        for x, y in coords:
            b += struct.pack("<2I", x, y)
        b += struct.pack("<I", mapflag)
    b += struct.pack("<I", black)
    if white is None:
        b += struct.pack("<I", 0)
    else:
        b += struct.pack("<2I", 1, white)
    if separate is None:
        b += struct.pack("<I", 0)
    else:
        b += struct.pack("<I", 1) + struct.pack("<4I", *separate)
    b += struct.pack("<I", len(areas))
    for is_vert, off, size in areas:
        b += struct.pack("<3I", is_vert, off, size)
    if table is None:
        b += struct.pack("<I", 0)
    else:
        dither, entries = table
        b += struct.pack("<2I", dither, len(entries))
        b += struct.pack("<%dH" % len(entries), *[min(65535, e) for e in entries])
    with open(os.path.join(out, name + ".bin"), "wb") as f:
        f.write(bytes(b))


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--out", required=True)
    args = p.parse_args()
    os.makedirs(args.out, exist_ok=True)

    w, h = 16, 16
    u16 = lcg_bytes(11, w * h * 1 * 2)
    u16_cpp3 = lcg_bytes(12, w * h * 3 * 2)
    f32 = lcg_bytes(13, w * h * 1 * 4)
    f32_cpp3 = lcg_bytes(14, w * h * 3 * 4)

    # U16, black+white set, no extras: pure scaleValues path.
    seed("pp_u16_scale", args.out, 0, 1, w, h, 1, u16, black=0, white=65535)
    # U16 + separate black levels (skips calculateBlackAreas).
    seed("pp_u16_separate", args.out, 0, 1, w, h, 1, u16, black=256,
         white=16383, separate=(0, 256, 0, 256))
    # U16 + black areas (histogram path -> calculateBlackAreas).
    seed("pp_u16_blackareas", args.out, 0, 1, w, h, 1, u16, black=0,
         white=4095, areas=[(0, 0, 4), (1, 8, 8)])
    # U16 + linearization table with dither.
    seed("pp_u16_table", args.out, 0, 1, w, h, 1, u16, black=0, white=1023,
         table=(1, list(range(1024))[-64:]))
    # U16 + bad pixels with map.
    seed("pp_u16_badpix_map", args.out, 0, 1, w, h, 1, u16, black=0,
         white=2047, badpix=(3, [(2, 2), (8, 8), (15, 15)], 1))
    # U16 + bad pixels without map (transferBadPixelsToMap creates it).
    seed("pp_u16_badpix_nomap", args.out, 0, 1, w, h, 1, u16, black=0,
         white=2047, badpix=(2, [(4, 4), (10, 3)], 0))
    # U16 + crop before everything.
    seed("pp_u16_crop", args.out, 0, 1, w, h, 1, u16, crop=(1, 1, 12, 12),
         black=64, white=4095)
    # cpp=3 (non-CFA) with table + separate black.
    seed("pp_u16_cpp3", args.out, 0, 3, w, h, 0, u16_cpp3, black=0,
         white=65535, separate=(100, 200, 300, 400),
         table=(0, [i * 1024 for i in range(1, 65)]))
    # F32 scale path.
    seed("pp_f32_scale", args.out, 1, 1, w, h, 1, f32, black=0, white=65535)
    # F32 + crop + bad pixels.
    seed("pp_f32_badpix", args.out, 1, 1, w, h, 1, f32, crop=(0, 0, 16, 16),
         black=0, white=65535, badpix=(2, [(5, 7), (9, 9)], 1))
    # F32 cpp=3.
    seed("pp_f32_cpp3", args.out, 1, 3, w, h, 0, f32_cpp3, black=128,
         white=32768)

    print("seeds written to", args.out)


if __name__ == "__main__":
    main()
