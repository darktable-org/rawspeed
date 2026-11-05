#!/usr/bin/env python3
"""Deterministic structured seed corpus generator for the librawspeed OSS-Fuzz targets.

Root causes addressed (own-code coverage report 2026-11-04):
* FujiDecompressorFuzzer never gets past the strict CreateRawImage/CreateCFA/
  FujiHeader preamble: its target file FujiDecompressor.cpp sits at 43/634
  (6.8%) lines project-wide, and the dedicated target contributes no deep
  lines at all. Random mutation cannot synthesize the required 5xu32 image
  preamble, the CFA blob and the 16-byte big-endian FujiHeader with its
  tight validation (block_size==0x300, dimensions/6, blocks_in_row, ...).
* RafDecoder.cpp (44.1%) is only fed by generic pipeline targets
  (RawParserFuzzer/FiffParserFuzzer/RawSpeedFuzzer); its uncompressed
  decodeRawInternal paths never receive a structurally valid RAF container
  (magic + FIFF offset layout + TIFF IFDs with FUJI_* tags).

Everything is constructed from the harness/parser sources:
* fuzz preamble: fuzz/librawspeed/fuzz/Common.cpp
  (u32 LE width/height/type/cpp/isCFA, then i32 LE CFA size + x-major CFA colors)
* FujiHeader: src/librawspeed/decompressors/FujiDecompressor.cpp
  (u16 BE signature 0x4953, then version/raw_type/raw_bits bytes and u16 BE
  dimensions; block_size fixed 0x300; blocks_in_row = rounded_width/0x300;
  total_lines = raw_height/6; strip block sizes as u32 BE + align padding)
* RAF container: src/librawspeed/parsers/FiffParser.cpp (offsets at 0x54/0x5C/0x64)
  + decoders/RafDecoder.cpp (isRAF magic, FUJI_* tags, isCompressed heuristic).

All seeds are deterministic (no external samples), small, and structurally
valid so the fuzzers start deep inside the parsers instead of the front door.
"""

import argparse
import os
import struct

FUJI_BLOCK_SIZE = 0x300

# CFAColor: RED=0 GREEN=1 BLUE=2 (metadata/ColorFilterArray.h)
R, G, B = 0, 1, 2

# X-Trans base pattern (phase (0,0)), from common/XTransPhase.h (X-Pro1).
XTRANS_BASE = [
    [G, G, R, G, G, B],
    [G, G, B, G, G, R],
    [B, R, G, R, B, G],
    [G, G, B, G, G, R],
    [G, G, R, G, G, B],
    [R, B, G, B, R, G],
]

# Bayer RGGB (BayerPhase::RGGB), 2x2, indexed [y][x].
BAYER_RGGB = [[R, G], [G, B]]


def lcg_bytes(seed, count):
    """Deterministic pseudo-random bytes (high bit entropy) for strip payloads."""
    out = bytearray()
    x = seed & 0x7FFFFFFF or 1
    for _ in range(count):
        x = (x * 1103515245 + 12345) & 0x7FFFFFFF
        out.append((x >> 16) & 0xFF)
    return bytes(out)


def create_raw_image(width, height, cpp=1, is_cfa=1):
    # RawImageType::UINT16 == 0
    return struct.pack("<5I", width, height, 0, cpp, is_cfa)


def create_cfa(grid):
    """CreateCFA blob: i32 LE width/height then x-major u32 LE colors."""
    h = len(grid)
    w = len(grid[0])
    out = struct.pack("<ii", w, h)
    for x in range(w):  # x-major, matching fuzz/Common.cpp loop order
        for y in range(h):
            out += struct.pack("<I", grid[y][x])
    return out


def fuji_header(raw_type, raw_bits, raw_width, raw_height):
    assert raw_width >= FUJI_BLOCK_SIZE and raw_width % 24 == 0
    assert 0 < raw_width <= 0x3000 and 0 < raw_height <= 0x3000
    assert raw_height % 6 == 0 and raw_height >= 6
    blocks_in_row = (raw_width + FUJI_BLOCK_SIZE - 1) // FUJI_BLOCK_SIZE
    raw_rounded_width = blocks_in_row * FUJI_BLOCK_SIZE
    total_lines = raw_height // 6
    assert 1 <= total_lines <= 0x800
    assert raw_bits in (14, 16)  # 12 throws by design
    assert raw_type in (0, 16)
    assert raw_rounded_width - raw_width < FUJI_BLOCK_SIZE
    return struct.pack(
        ">HBBBHHHHBH",
        0x4953,          # signature ('IS')
        1,               # version
        raw_type,        # 0: Bayer, 16: X-Trans
        raw_bits,
        raw_height,
        raw_rounded_width,
        raw_width,
        FUJI_BLOCK_SIZE,
        blocks_in_row,
        total_lines,
    ), blocks_in_row


def fuji_payload(raw_type, raw_bits, width, height, strip_len):
    """Full FujiDecompressorFuzzer input: preamble + CFA + header + strips."""
    grid = BAYER_RGGB if raw_type == 0 else XTRANS_BASE
    header, blocks_in_row = fuji_header(raw_type, raw_bits, width, height)
    out = create_raw_image(width, height)
    out += create_cfa(grid)
    out += header
    out += struct.pack(">%dI" % blocks_in_row, *([strip_len] * blocks_in_row))
    raw_offset = 4 * blocks_in_row
    if raw_offset & 0xC:
        out += b"\x00" * (0x10 - (raw_offset & 0xC))  # align padding, see ctor
    for n in range(blocks_in_row):
        out += lcg_bytes(0xC0FFEE + 7919 * n + 31 * raw_bits + raw_type, strip_len)
    return out


def tiff_ifd(entries, next_ifd=0):
    """Little-endian classic TIFF: header + one IFD. entries: (tag,type,count,value)."""
    typesize = {2: 1, 3: 2, 4: 4}
    fmt = {2: "B", 3: "H", 4: "I"}
    n = len(entries)
    heap_base = 8 + 2 + 12 * n + 4
    blob = bytearray()
    heap = bytearray()
    blob += b"II" + struct.pack("<HI", 42, 8)
    blob += struct.pack("<H", n)
    for tag, typ, count, value in entries:
        size = typesize[typ] * count
        if size <= 4:
            if isinstance(value, bytes):
                packed = value
            else:
                packed = struct.pack("<" + fmt[typ], value)
            blob += struct.pack("<HHI", tag, typ, count) + packed.ljust(4, b"\x00")
        else:
            blob += struct.pack("<HHII", tag, typ, count, heap_base + len(heap))
            if isinstance(value, bytes):
                heap += value
            else:
                heap += struct.pack("<" + fmt[typ], value)
    blob += struct.pack("<I", next_ifd)
    blob += heap
    return bytes(blob)


def raf_container(width, height, strip, bps_tag, iso=200):
    """Full RAF container: magic + FIFF offsets + root TIFF + raw TIFF + data.

    strip: raw payload bytes placed after the raw TIFF; byte count from len().
    """
    tiff_at = 0x100
    second_ifd = 0x200
    data_at = 0x300

    make = b"FUJIFILM\x00"
    model = b"X-T3\x00"
    root = tiff_ifd([
        (0x010F, 2, len(make), make),          # MAKE
        (0x0110, 2, len(model), model),        # MODEL
        (0x8827, 3, 1, iso),                   # ISOSPEEDRATINGS
    ])

    raw = tiff_ifd([
        (0xF001, 4, 1, width),                 # FUJI_RAWIMAGEFULLWIDTH
        (0xF002, 4, 1, height),                # FUJI_RAWIMAGEFULLHEIGHT
        (0xF003, 4, 1, bps_tag),               # FUJI_BITSPERSAMPLE
        (0xF007, 4, 1, data_at - second_ifd),  # FUJI_STRIPOFFSETS (root-relative)
        (0xF008, 4, 1, len(strip)),            # FUJI_STRIPBYTECOUNTS
    ])
    assert len(root) <= second_ifd - tiff_at
    assert len(raw) <= data_at - second_ifd

    buf = bytearray(data_at + len(strip))
    buf[0:16] = b"FUJIFILMCCD-RAW "                  # RafDecoder::isRAF magic
    struct.pack_into(">I", buf, 0x54, tiff_at - 12)  # first_ifd (+12 in parser)
    struct.pack_into(">I", buf, 0x5C, tiff_at + 8)   # third_ifd (unused)
    struct.pack_into(">I", buf, 0x64, second_ifd)    # second_ifd (raw TIFF)
    buf[tiff_at:tiff_at + len(root)] = root
    buf[second_ifd:second_ifd + len(raw)] = raw
    buf[data_at:data_at + len(strip)] = strip
    return bytes(buf)


def uncompressed_strip(width, height):
    """16-bit bucket strip: 2*w*h bytes -> 8c/(wh) == 16 == bps, not compressed."""
    return lcg_bytes(0xBEEF, width * height * 2)


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--out", required=True, help="directory for seed subfolders")
    args = p.parse_args()

    fuji_dir = os.path.join(args.out, "fuji")
    raf_dir = os.path.join(args.out, "raf")
    os.makedirs(fuji_dir, exist_ok=True)
    os.makedirs(raf_dir, exist_ok=True)

    # --- FujiDecompressorFuzzer (direct harness) ---------------------------
    fuji = {
        # Bayer 14-bit, single block, one 6-row line: the minimal deep decode.
        "fuji_bayer14_min": (0, 14, 768, 6, FUJI_BLOCK_SIZE),
        # Bayer 14-bit, two lines.
        "fuji_bayer14_two_lines": (0, 14, 768, 12, FUJI_BLOCK_SIZE),
        # Bayer 16-bit.
        "fuji_bayer16": (0, 16, 768, 6, FUJI_BLOCK_SIZE),
        # X-Trans (6x6 CFA), 14-bit.
        "fuji_xtrans14": (16, 14, 768, 6, FUJI_BLOCK_SIZE),
        # X-Trans, 16-bit, two blocks in a row.
        "fuji_xtrans16_multiblock": (16, 16, 1536, 12, FUJI_BLOCK_SIZE),
        # Truncated strips: exercise the decode loop's EOS/error paths.
        "fuji_bayer14_trunc": (0, 14, 768, 6, FUJI_BLOCK_SIZE // 4),
        "fuji_xtrans14_trunc": (16, 14, 1536, 6, FUJI_BLOCK_SIZE // 8),
    }
    for name, (rt, rb, w, h, sl) in fuji.items():
        with open(os.path.join(fuji_dir, name + ".raw"), "wb") as f:
            f.write(fuji_payload(rt, rb, w, h, sl))

    # --- RAF pipeline containers -------------------------------------------
    # Uncompressed 16-bit bucket (8c/(w*h)==16, not "compressed").
    with open(os.path.join(raf_dir, "raf_uncompressed16.raw"), "wb") as f:
        w, h = 64, 8
        f.write(raf_container(w, h, uncompressed_strip(w, h), bps_tag=16))

    # Uncompressed 14-bit bucket (14wh <= 8c < 28wh bytes).
    with open(os.path.join(raf_dir, "raf_uncompressed14.raw"), "wb") as f:
        w, h = 64, 6
        strip = lcg_bytes(0xD00D, (7 * w * h) // 4 + 8)
        f.write(raf_container(w, h, strip, bps_tag=14))

    # Uncompressed 12-bit bucket (12wh <= 8c < 24wh bytes).
    with open(os.path.join(raf_dir, "raf_uncompressed12.raw"), "wb") as f:
        w, h = 64, 6
        strip = lcg_bytes(0xF00D, (3 * w * h) // 2 + 8)
        f.write(raf_container(w, h, strip, bps_tag=12))

    # Compressed RAF: reaches RafDecoder's compressed branch and the
    # FujiDecompressor constructor (header parse + validation); the pipeline
    # targets carry no camera CFA so the ctor throws after parsing the header.
    with open(os.path.join(raf_dir, "raf_compressed.raw"), "wb") as f:
        w, h = 768, 6
        hdr, blocks = fuji_header(0, 14, w, h)
        strip = hdr + struct.pack(">%dI" % blocks, *([FUJI_BLOCK_SIZE] * blocks))
        if (4 * blocks) & 0xC:
            strip += b"\x00" * (0x10 - ((4 * blocks) & 0xC))
        strip += lcg_bytes(0x5EED, FUJI_BLOCK_SIZE)
        f.write(raf_container(w, h, strip, bps_tag=14))

    print("seeds written to", args.out)


if __name__ == "__main__":
    main()
