#!/usr/bin/env python3
"""Deterministic seeds for the new Cr2sRawInterpolatorFuzzer (component fuzzer
for the Canon sRaw YCbCr->RGB interpolation stage).

Input layout (little-endian, see fuzz/librawspeed/interpolators/Cr2sRawInterpolator.cpp):
  u32 version (0/1/2), u32 subsampling (0=4:2:2, 1=4:2:0), u32 hue index,
  u32 raw width, u32 raw height, 3 x u16 sRaw coefficients, then u16 pixels.
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


def seed(version, ss_sel, hue_idx, w_raw, h_raw, coeffs, pixel_seed):
    pixels = lcg_bytes(pixel_seed, 64 * 64 * 2)  # upper bound; harness stops at EOF
    return (struct.pack("<3I", version, ss_sel, hue_idx) +
            struct.pack("<2I", w_raw, h_raw) +
            struct.pack("<3H", *coeffs) +
            pixels)


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--out", required=True)
    args = p.parse_args()
    os.makedirs(args.out, exist_ok=True)

    seeds = {
        # 4:2:2, version 0 (EOS 40D), minimal 2-MCU width.
        "cr2sraw_422_v0_min": seed(0, 0, 0, 1, 1, (1024, 1024, 1024), 1),
        # 4:2:2, version 1 (default algorithm).
        "cr2sraw_422_v1": seed(1, 0, 2, 2, 3, (926, 1096, 926), 2),
        # 4:2:2, version 2 (EOS 5D Mk III).
        "cr2sraw_422_v2": seed(2, 0, 1, 3, 2, (1024, 1024, 1024), 3),
        # 4:2:0, version 1.
        "cr2sraw_420_v1": seed(1, 1, 4 % 4, 2, 1, (926, 1096, 926), 4),
        # 4:2:0, version 2.
        "cr2sraw_420_v2": seed(2, 1, 0, 3, 2, (1024, 1024, 1024), 5),
        # Maximal (attacker-achievable) coefficients: fixed-point overflow hunt.
        "cr2sraw_422_v1_maxcoeff": seed(1, 0, 2, 2, 2, (0xFFFF, 0xFFFF, 0xFFFF), 6),
        "cr2sraw_420_v1_maxcoeff": seed(1, 1, 4 % 4, 2, 2, (0xFFFF, 0xFFFF, 0xFFFF), 7),
    }
    for name, blob in seeds.items():
        with open(os.path.join(args.out, name + ".bin"), "wb") as f:
            f.write(blob)
    print("seeds written to", args.out)


if __name__ == "__main__":
    main()
