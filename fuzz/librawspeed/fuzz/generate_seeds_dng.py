#!/usr/bin/env python3
"""Deterministic DNG seeds reaching the newly compiled-in DeflateDecompressor
and JpegDecompressor (build-config pair librawspeed-build-config-zlib-jpeg:
WITH_ZLIB/WITH_JPEG ON).

Minimal DNG: a classic little-endian TIFF whose only IFD carries the DNGVersion
tag (selects DngDecoder in TiffParser::makeDecoder), PhotometricInterpretation
1 (non-CFA, skips parseCFA) and one strip of payload data. The pipeline targets
(RawSpeedFuzzer, RawParserFuzzer-GetDecoder-Decode) parse the container and run
the decompressor end-to-end.

Two payload families:
- Compression 8 (Adobe deflate) on SAMPLEFORMAT=3 float data -> runs the
  deflate decompressor end-to-end (predictors 3 / 34894 / 34895); an integer
  SAMPLEFORMAT=1 variant is rejected by decodeData (error path).
- Compression 0x884C (lossy JPEG) on SAMPLEFORMAT=1 uint16 data -> hands a
  hand-crafted minimal baseline JPEG (Annex-K Huffman tables, all-zero
  coefficient blocks) to JpegDecompressor, which feeds it to libjpeg.
  Grayscale 8x8-in-16x16 (copy clamp), 3-component 16x16 (multi-component copy
  loop), an entropy-truncated variant (jpeg_read_scanlines failure path) and a
  corrupt SOF0-length variant (jpeg_read_header failure path).
"""

import argparse
import os
import struct
import zlib


def lcg_bytes(seed, count):
    out = bytearray()
    x = seed & 0x7FFFFFFF or 1
    for _ in range(count):
        x = (x * 1103515245 + 12345) & 0x7FFFFFFF
        out.append((x >> 16) & 0xFF)
    return bytes(out)


# Annex K luminance Huffman tables (JPEG standard, ITU T.81 Table K.3/K.4).
_DC_BITS = [0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0]
_DC_VALS = list(range(12))
_AC_BITS = [0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 125]
_AC_VALS = [
    0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06,
    0x13, 0x51, 0x61, 0x07, 0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xA1, 0x08,
    0x23, 0x42, 0xB1, 0xC1, 0x15, 0x52, 0xD1, 0xF0, 0x24, 0x33, 0x62, 0x72,
    0x82, 0x09, 0x0A, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2A, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x43, 0x44, 0x45,
    0x46, 0x47, 0x48, 0x49, 0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
    0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x73, 0x74, 0x75,
    0x76, 0x77, 0x78, 0x79, 0x7A, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
    0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3,
    0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6,
    0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9,
    0xCA, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xE1, 0xE2,
    0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xF1, 0xF2, 0xF3, 0xF4,
    0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA,
]
assert len(_AC_VALS) == sum(_AC_BITS) == 162
assert sum(_DC_BITS) == len(_DC_VALS) == 12


def _marker(marker, payload):
    return bytes([0xFF, marker]) + struct.pack(">H", len(payload) + 2) + payload


def _zero_block_entropy(n_blocks):
    """Entropy stream for n_blocks of all-zero coefficients with the Annex K
    luminance tables: DC diff category 0 ('00') + EOB ('1010') per block,
    padded with 1 bits. Contains no 0xFF bytes, so no byte stuffing."""
    bits = "001010" * n_blocks
    bits += "1" * ((-len(bits)) % 8)
    return int(bits, 2).to_bytes(len(bits) // 8, "big")


def baseline_jpeg(width, height, components):
    """Minimal deterministic baseline JPEG: `components` components at 1x1
    sampling, uniform quant table, Annex K luminance Huffman tables for every
    component, all-zero coefficient blocks."""
    n_blocks = ((width + 7) // 8) * ((height + 7) // 8) * components
    comps = ([(1, 0x11, 0)] if components == 1
             else [(1, 0x11, 0), (2, 0x11, 0), (3, 0x11, 0)])

    out = bytearray(b"\xff\xd8")  # SOI
    # DQT: Pq=0/Tq=0, uniform values (order irrelevant for a uniform table).
    out += _marker(0xDB, b"\x00" + bytes([16] * 64))
    # SOF0: baseline, 8-bit precision.
    sof = bytes([8]) + struct.pack(">HH", height, width) + bytes([len(comps)])
    for cid, hv, tq in comps:
        sof += bytes([cid, hv, tq])
    out += _marker(0xC0, sof)
    # DHT: DC table 0 and AC table 0, shared by all components.
    out += _marker(0xC4, bytes([0x00] + _DC_BITS + _DC_VALS))
    out += _marker(0xC4, bytes([0x10] + _AC_BITS + _AC_VALS))
    # SOS: all components use DC table 0 / AC table 0.
    sos = bytes([len(comps)])
    for cid, _, _ in comps:
        sos += bytes([cid, 0x00])
    sos += bytes([0x00, 63, 0x00])  # Ss=0, Se=63, Ah/Al=0
    out += _marker(0xDA, sos)
    out += _zero_block_entropy(n_blocks)
    out += b"\xff\xd9"  # EOI
    return bytes(out)


def dng_container(path, width, height, compression, sample_format, bps, spp,
                  payload, predictor=None):
    """Minimal single-IFD DNG selecting DngDecoder, one strip = `payload`.

    Entries are emitted in ascending tag order (as real TIFF writers do).
    """
    entries = [
        (0x0100, 4, 1, width),            # IMAGEWIDTH
        (0x0101, 4, 1, height),           # IMAGELENGTH
        (0x0102, 3, 1, bps),              # BITSPERSAMPLE
        (0x0103, 3, 1, compression),      # COMPRESSION
        (0x0106, 3, 1, 1),                # PHOTOMETRICINTERPRETATION: BlackIsZero
        (0x0111, 4, 1, None),             # STRIPOFFSETS (patched)
        (0x0115, 3, 1, spp),              # SAMPLESPERPIXEL
        (0x0116, 4, 1, height),           # ROWSPERSTRIP
        (0x0117, 4, 1, len(payload)),     # STRIPBYTECOUNTS
    ]
    if predictor is not None:
        entries.append((0x0131, 3, 1, predictor))  # PREDICTOR (deflate only)
    entries += [
        (0x0153, 3, 1, sample_format),    # SAMPLEFORMAT
        (0xC612, 1, 4, bytes([1, 4, 0, 0])),  # DNGVERSION (selects DngDecoder)
    ]
    n = len(entries)
    ifd_at = 8
    ifd_size = 2 + 12 * n + 4
    blob_at = ifd_at + ifd_size

    buf = bytearray(blob_at + len(payload))
    buf[0:4] = b"II" + struct.pack("<HI", 42, 8)
    buf[ifd_at:ifd_at + 2] = struct.pack("<H", n)
    off = ifd_at + 2
    for tag, typ, count, value in entries:
        buf[off:off + 2] = struct.pack("<H", tag)
        buf[off + 2:off + 4] = struct.pack("<H", typ)
        buf[off + 4:off + 8] = struct.pack("<I", count)
        if isinstance(value, bytes):
            buf[off + 8:off + 12] = value.ljust(4, b"\x00")
        elif tag == 0x0111:
            buf[off + 8:off + 12] = struct.pack("<I", blob_at)
        else:
            fmt = {1: "<B", 2: "<B", 3: "<H", 4: "<I"}[typ]
            packed = struct.pack(fmt, value)
            buf[off + 8:off + 12] = packed.ljust(4, b"\x00")
        off += 12
    buf[off:off + 4] = struct.pack("<I", 0)  # next IFD
    buf[blob_at:blob_at + len(payload)] = payload
    with open(path, "wb") as f:
        f.write(bytes(buf))


def dng_deflate(path, width, height, sample_format, bps, pixel_seed, predictor):
    if sample_format == 1:
        assert bps == 16
        raw = lcg_bytes(pixel_seed, width * height * 2)
    else:
        assert sample_format == 3 and bps == 32
        raw = lcg_bytes(pixel_seed, width * height * 4)
    blob = zlib.compress(raw, 9)
    dng_container(path, width, height, 8, sample_format, bps, 1, blob,
                  predictor=predictor)


def dng_lossyjpeg(path, width, height, spp, jpeg):
    """Lossy-JPEG DNG: SAMPLEFORMAT=1 (uint16) is required for Compression
    0x884C; the JPEG component count must match SAMPLESPERPIXEL."""
    dng_container(path, width, height, 0x884C, 1, 16, spp, jpeg)


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--out", required=True)
    args = p.parse_args()
    os.makedirs(args.out, exist_ok=True)
    # Decodable deflate DNGs: SAMPLEFORMAT=3 (float) is required for
    # Compression 8; predictors 3/34894/34895 are accepted.
    dng_deflate(os.path.join(args.out, "dng_deflate_f32.tiff"), 32, 16, 3, 32,
                0xD3, 3)
    dng_deflate(os.path.join(args.out, "dng_deflate_f32_wide.tiff"), 64, 16, 3,
                32, 0xD4, 34895)
    dng_deflate(os.path.join(args.out, "dng_deflate_f32_p34894.tiff"), 32, 32,
                3, 32, 0xD5, 34894)
    # Integer + deflate: rejected by decodeData (error-path seed).
    dng_deflate(os.path.join(args.out, "dng_deflate_u16_errpath.tiff"), 32, 32,
                1, 16, 0xD1, 3)
    # Lossy-JPEG DNGs (grayscale JPEG smaller than the raw: copy clamp;
    # 3-component JPEG at full size: multi-component copy loop).
    dng_lossyjpeg(os.path.join(args.out, "dng_lossyjpeg_gray.tiff"), 16, 16, 1,
                  baseline_jpeg(8, 8, 1))
    dng_lossyjpeg(os.path.join(args.out, "dng_lossyjpeg_rgb.tiff"), 16, 16, 3,
                  baseline_jpeg(16, 16, 3))
    # Entropy truncated after SOS: jpeg_read_scanlines fails (error path).
    dng_lossyjpeg(os.path.join(args.out, "dng_lossyjpeg_trunc.tiff"), 16, 16, 1,
                  baseline_jpeg(8, 8, 1)[:-2])
    # Corrupt SOF0 marker length: jpeg_read_header fails (error path).
    bad = bytearray(baseline_jpeg(8, 8, 1))
    sof_at = bad.index(b"\xff\xc0")
    bad[sof_at + 2:sof_at + 4] = struct.pack(">H", 0xFFFF)
    dng_lossyjpeg(os.path.join(args.out, "dng_lossyjpeg_badhdr.tiff"), 16, 16,
                  1, bytes(bad))
    print("seeds written to", args.out)


if __name__ == "__main__":
    main()
